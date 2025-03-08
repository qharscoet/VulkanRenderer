#include "FileUtils.h"

#include <fstream>
#include <unordered_map>

//#define STB_IMAGE_IMPLEMENTATION
//#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
//#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include <tiny_obj_loader.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include <tiny_gltf.h>

std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}


Texture loadTexture(const char* path) {
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	size_t imageSize = texWidth * texHeight * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	std::vector<unsigned char> pixel_data;
	pixel_data.resize(imageSize);
	memcpy(&pixel_data[0], pixels, imageSize);

	stbi_image_free(pixels);
	return { (uint32_t)texWidth, (uint32_t)texHeight, texChannels, pixel_data, imageSize };
}


void freeTexturePixels(Texture* tex)
{
	//stbi_image_free(tex->pixels);
	//tex->pixels = nullptr;
}

void loadObj(const char* path, Mesh* out_mesh)
{
	tinyobj::ObjReaderConfig reader_config;
	reader_config.mtl_search_path = "./"; // Path to material files

	tinyobj::ObjReader reader;

	if (!reader.ParseFromFile(path, reader_config))
	{
		throw std::runtime_error("can't load model : " + reader.Error());
	}

	union idx_hash {
		struct {
			uint16_t pos;
			uint16_t texCood;
			uint16_t normal;
			uint16_t color;
		};
		uint64_t val;
	};

	std::unordered_map<uint64_t, uint32_t> uniqueVertices{};

	auto& attrib = reader.GetAttrib();

	for (const auto& shape : reader.GetShapes())
	{
		for (const auto& index : shape.mesh.indices) {
			MeshVertex vertex{};

			vertex.pos[0] = attrib.vertices[3 * index.vertex_index + 0];
			vertex.pos[1] = attrib.vertices[3 * index.vertex_index + 1];
			vertex.pos[2] = attrib.vertices[3 * index.vertex_index + 2];

			if (index.texcoord_index >= 0)
			{
				vertex.texCoord[0] = attrib.texcoords[2 * index.texcoord_index + 0];
				vertex.texCoord[1] = 1.0f - attrib.texcoords[2 * index.texcoord_index + 1];
			}

			if (index.normal_index >= 0)
			{
				vertex.normals[0] = attrib.normals[3 * index.normal_index + 0];
				vertex.normals[1] = attrib.normals[3 * index.normal_index + 1];
				vertex.normals[2] = attrib.normals[3 * index.normal_index + 2];
			}

			vertex.color[0] = 1.0f;
			vertex.color[1] = 1.0f;
			vertex.color[2] = 1.0f;

			//TODO: careful components are uint16 so collision is likely for models of more that 65K indices
			idx_hash to_hash;
			to_hash.pos = index.vertex_index;
			to_hash.texCood = index.texcoord_index;
			to_hash.normal = index.normal_index;

			if (uniqueVertices.count(to_hash.val) == 0)
			{
				uniqueVertices[to_hash.val] = out_mesh->vertices.size();
				out_mesh->vertices.push_back(vertex);
			}


			out_mesh->indices.push_back(uniqueVertices[to_hash.val]);

		}

	}
}

size_t get_accessor_elem_size(const tinygltf::Accessor& accessor)
{
	int elem_size = 0;
	switch (accessor.componentType)
	{
	case TINYGLTF_COMPONENT_TYPE_FLOAT: elem_size = sizeof(float); break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: elem_size = sizeof(uint8_t); break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: elem_size = sizeof(uint16_t); break;
	default: break;
	}

	int elem_count = 0;
	switch (accessor.type)
	{
	case TINYGLTF_TYPE_SCALAR: elem_count = 1; break;
	case TINYGLTF_TYPE_VEC2: elem_count = 2; break;
	case TINYGLTF_TYPE_VEC3: elem_count = 3; break;
	}

	return elem_size * elem_count;
}


struct AttributeInfo {
	const uint8_t* start_addr;
	const size_t elem_size;
	const size_t count;
	const size_t stride;
	const double maxRange;
};
const std::optional<AttributeInfo> get_accessor_start_addr(const tinygltf::Model& model, size_t mesh_idx, std::string attr)
{
	const tinygltf::Mesh& m = model.meshes[mesh_idx];

	if (!m.primitives[0].attributes.contains(attr))
		return {};

	int accessor_idx = m.primitives[0].attributes.at(attr);
	const tinygltf::Accessor& accessor = model.accessors[accessor_idx];
	const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
	const uint8_t* data = model.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + accessor.byteOffset;

	size_t elem_size = get_accessor_elem_size(accessor);
	size_t stride = bufferView.byteStride > 0 ? bufferView.byteStride : elem_size;

	double maxRange = 0.0f;
	for (int i = 0; i < accessor.minValues.size(); i++)
	{
		const double min = accessor.minValues[i];
		const double max = accessor.maxValues[i];
		const double range = max - min;

		maxRange = std::max(range, maxRange);
	}
	


	return AttributeInfo{ data, elem_size, accessor.count, stride, maxRange};
}

int loadGltf(const char* path, Mesh* out_mesh)
{

	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);
	//bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]); // for binary glTF(.glb)

	if (!warn.empty()) {
		printf("Warn: %s\n", warn.c_str());
	}

	if (!err.empty()) {
		printf("Err: %s\n", err.c_str());
	}

	if (!ret) {
		printf("Failed to parse glTF\n");
		return -1;
	}

	if (model.meshes.size() > 0)
	{
		const tinygltf::Mesh& m = model.meshes[0];
		
		{
			/*int vertices_accessor_idx = m.primitives[0].attributes.at("POSITION");
			const tinygltf::Accessor& vertices_accessor = model.accessors[vertices_accessor_idx];
			const tinygltf::BufferView& vertices_bufferView = model.bufferViews[accessor.bufferView];
			auto vertices_data = model.buffers[vertices_bufferView.buffer].data.data() + vertices_bufferView.byteOffset + vertices_accessor.byteOffset;
	
			size_t elem_size = get_accessor_elem_size(vertices_accessor);
			size_t stride = vertices_bufferView.byteStride > 0 ? vertices_bufferView.byteStride : elem_size;
			*/

			const std::optional<AttributeInfo> pos_info = get_accessor_start_addr(model, 0, "POSITION");
			const std::optional<AttributeInfo> texCoords_info = get_accessor_start_addr(model, 0, "TEXCOORD_0");
			const std::optional<AttributeInfo> color_info = get_accessor_start_addr(model, 0, "COLOR_0");
			const std::optional<AttributeInfo> normal_info = get_accessor_start_addr(model, 0, "NORMAL");
			
			if (!pos_info.has_value())
				return -1;

			AttributeInfo positions = *pos_info;

			for (int i = 0; i < positions.count; i++)
			{
				MeshVertex v = {};

				memcpy(v.pos, positions.start_addr + i * positions.stride, positions.elem_size);

				if (positions.maxRange > 0)
				{
					const float factor = 1.0f / positions.maxRange;
					v.pos[0] *= factor;
					v.pos[1] *= factor;
					v.pos[2] *= factor;
				}

				if (normal_info.has_value())
					memcpy(v.normals, normal_info->start_addr + i * normal_info->stride, normal_info->elem_size);

				if (texCoords_info.has_value())
					memcpy(v.texCoord, texCoords_info->start_addr + i * texCoords_info->stride, texCoords_info->elem_size);
				
				if (color_info.has_value())
					memcpy(v.color, color_info->start_addr + i * color_info->stride, color_info->elem_size);
				else
				{
					v.color[0] = 1.0f;
					v.color[1] = 1.0f;
					v.color[2] = 1.0f;
				}

				out_mesh->vertices.push_back(v);
			}
		}

		{
			int indices_accessor_idx = m.primitives[0].indices;
			const tinygltf::Accessor& accessor = model.accessors[indices_accessor_idx];
			const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
			auto data = model.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + accessor.byteOffset;

			size_t elem_size = get_accessor_elem_size(accessor);
			size_t stride = bufferView.byteStride > 0 ? bufferView.byteStride : elem_size;
			for (int i = 0; i < accessor.count; i++)
			{
				//TODO: read the format correctly;
				uint16_t* addr = (uint16_t*)(data + i * stride);
				out_mesh->indices.push_back(*addr);
			}

		}

		for (const tinygltf::Texture& tex : model.textures)
		{
			tinygltf::Image& img = model.images[tex.source];

			Texture t;
			t.height = img.height;
			t.width = img.width;
			t.channels = img.component;
			t.size = img.width * img.height * img.component ;
			t.pixels = img.image;
			t.name = img.name;

			out_mesh->textures.push_back(t);
			
		}

		const tinygltf::Material& material = model.materials[m.primitives[0].material];

		out_mesh->material = {
			.baseColor = material.pbrMetallicRoughness.baseColorTexture.index,
			.mettalicRoughness = material.pbrMetallicRoughness.metallicRoughnessTexture.index,
			.normal = material.normalTexture.index,
			.emissive = material.emissiveTexture.index,
			.occlusion = material.occlusionTexture.index,
		};

	}

	return 0;
}