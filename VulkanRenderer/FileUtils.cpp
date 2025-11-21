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


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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

	auto load_lambda = []<typename T>(const char* path, int* width, int* height, int* channels) -> std::vector<T> {

		T* pixels = nullptr;
		if constexpr (std::is_same_v<T, unsigned char>) {
			pixels = stbi_load(path, width, height, channels, STBI_rgb_alpha);
			if (!pixels) {
				throw std::runtime_error("failed to load texture image!");
			}
		}
		else if constexpr (std::is_same_v<T, float>) {
			pixels = stbi_loadf(path, width, height, channels, STBI_rgb_alpha);
			if (!pixels) {
				throw std::runtime_error("failed to load HDR texture image!");
			}
		}
		else {
			static_assert("Unsupported pixel type");
		}

		size_t imageSize = (*width) * (*height) * /*(*channels)*/ 4; //Force 4 channels
		std::vector<T> pixel_data;
		pixel_data.resize(imageSize);
		
		memcpy(&pixel_data[0], pixels, imageSize * sizeof(T));
		*channels = 4;
	
		stbi_image_free(pixels);

		return pixel_data;
	};

	Texture::PixelData pixel_data;

	if (stbi_is_hdr(path))
	{
		pixel_data = std::move(load_lambda.template operator() < float > (path, &texWidth, &texHeight, &texChannels));
	}
	else
	{
		pixel_data = std::move(load_lambda.template operator() < unsigned char > (path, &texWidth, &texHeight, &texChannels));
	}

	size_t imageSize = texWidth * texHeight * texChannels * (stbi_is_hdr(path) ? sizeof(float) : sizeof(unsigned char));


	return {
		.height = (uint32_t)texHeight,
		.width = (uint32_t)texWidth,
		.channels = texChannels,
		.size = imageSize,
		.is_srgb = true,
		.is_cubemap = false,
		.pixels = std::move(pixel_data),
		.name = std::string(path)
	};
}


void freeTexturePixels(Texture* tex)
{
	//stbi_image_free(tex->pixels);
	//tex->pixels = nullptr;
}

Texture loadCubemapTexture(const std::array<const char*, 6>& face_paths)
{
	std::vector<unsigned char> pixels;
	int texWidth = 0;
	int texHeight = 0;
	int texChannels = 0;
	size_t faceSize = 0;
	for (int i =  0; i < 6; i++)
	{
		const char* face = face_paths[i];
		int width, height, channels;
		stbi_uc* face_pixels = stbi_load(face, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!face_pixels)
		{
			throw std::runtime_error("failed to load texture image!");
		}
		if (pixels.empty())
		{
			faceSize = texWidth * texHeight * texChannels;
			pixels.resize(faceSize * 6);
		}

		memcpy(&pixels[i * faceSize], face_pixels, faceSize);
		stbi_image_free(face_pixels);
	}
	return {
		.height = (uint32_t)texHeight,
		.width = (uint32_t)texWidth,
		.channels = texChannels,
		.size = faceSize * 6,
		.is_srgb = false,
		.is_cubemap = true,
		.pixels = std::move(pixels)
	};
}

//TODO : get rid of glm here
void ComputeTangents(std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices) {
	// Temporary containers to store tangents
	std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.0f));

	// Iterate over each triangle
	for (size_t i = 0; i < indices.size(); i += 3) {
		// Get triangle indices
		uint32_t i0 = indices[i];
		uint32_t i1 = indices[i + 1];
		uint32_t i2 = indices[i + 2];

		// Get triangle vertices
		MeshVertex& v0 = vertices[i0];
		MeshVertex& v1 = vertices[i1];
		MeshVertex& v2 = vertices[i2];

		// Compute edge vectors
		glm::vec3 edge1 = glm::make_vec3(&v1.pos[0]) - glm::make_vec3(&v0.pos[0]);
		glm::vec3 edge2 = glm::make_vec3(&v2.pos[0]) - glm::make_vec3(&v0.pos[0]);

		// Compute delta UVs
		glm::vec2 deltaUV1 = glm::make_vec2(&v1.texCoord[0]) - glm::make_vec2(&v0.texCoord[0]);
		glm::vec2 deltaUV2 = glm::make_vec2(&v2.texCoord[0]) - glm::make_vec2(&v0.texCoord[0]);

		// Compute tangent
		float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
		glm::vec3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);

		// Accumulate tangents
		tangents[i0] += tangent;
		tangents[i1] += tangent;
		tangents[i2] += tangent;
	}

	// Normalize and store tangents
	for (size_t i = 0; i < vertices.size(); ++i) {
		glm::vec3 T = glm::normalize(tangents[i]);
		glm::vec3 norm = glm::make_vec3(&vertices[i].normals[0]);

		// Ensure the tangent is orthogonal to the normal
		T = glm::normalize(T - norm * glm::dot(norm, T));

		// Compute handedness (w component)
		glm::vec3 bitangent = glm::cross(norm, T);
		float handedness = (glm::dot(bitangent, T) < 0.0f) ? -1.0f : 1.0f;

		// Store tangent in vertex
		vertices[i].tangent[0] = T.x;
		vertices[i].tangent[1] = T.y;
		vertices[i].tangent[2] = T.z;
		vertices[i].tangent[3] = handedness;
	}
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

	ComputeTangents(out_mesh->vertices, out_mesh->indices);
}

size_t get_accessor_elem_size(const tinygltf::Accessor& accessor)
{
	int elem_size = 0;
	switch (accessor.componentType)
	{
	case TINYGLTF_COMPONENT_TYPE_FLOAT: elem_size = sizeof(float); break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: elem_size = sizeof(uint8_t); break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: elem_size = sizeof(uint16_t); break;
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: elem_size = sizeof(uint32_t); break;
	default: break;
	}

	int elem_count = 0;
	switch (accessor.type)
	{
	case TINYGLTF_TYPE_SCALAR: elem_count = 1; break;
	case TINYGLTF_TYPE_VEC2: elem_count = 2; break;
	case TINYGLTF_TYPE_VEC3: elem_count = 3; break;
	case TINYGLTF_TYPE_VEC4: elem_count = 4; break;
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
const std::optional<AttributeInfo> get_accessor_start_addr(const tinygltf::Model& model, size_t mesh_idx, size_t primitive_idx, std::string attr)
{
	const tinygltf::Mesh& m = model.meshes[mesh_idx];

	if (!m.primitives[primitive_idx].attributes.contains(attr))
		return {};

	int accessor_idx = m.primitives[primitive_idx].attributes.at(attr);
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

void loadGltfMesh(const tinygltf::Model& model, size_t mesh_idx, size_t primitive_idx, Mesh* out_mesh)
{
	const tinygltf::Mesh& m = model.meshes[mesh_idx];
	bool has_tangent = false;

	{
		/*int vertices_accessor_idx = m.primitives[0].attributes.at("POSITION");
		const tinygltf::Accessor& vertices_accessor = model.accessors[vertices_accessor_idx];
		const tinygltf::BufferView& vertices_bufferView = model.bufferViews[accessor.bufferView];
		auto vertices_data = model.buffers[vertices_bufferView.buffer].data.data() + vertices_bufferView.byteOffset + vertices_accessor.byteOffset;

		size_t elem_size = get_accessor_elem_size(vertices_accessor);
		size_t stride = vertices_bufferView.byteStride > 0 ? vertices_bufferView.byteStride : elem_size;
		*/

		const std::optional<AttributeInfo> pos_info = get_accessor_start_addr(model, mesh_idx, primitive_idx, "POSITION");
		const std::optional<AttributeInfo> texCoords_info = get_accessor_start_addr(model, mesh_idx, primitive_idx, "TEXCOORD_0");
		const std::optional<AttributeInfo> color_info = get_accessor_start_addr(model, mesh_idx, primitive_idx, "COLOR_0");
		const std::optional<AttributeInfo> normal_info = get_accessor_start_addr(model, mesh_idx, primitive_idx, "NORMAL");
		const std::optional<AttributeInfo> tangent_info = get_accessor_start_addr(model, mesh_idx, primitive_idx, "TANGENT");

		if (!pos_info.has_value())
			return;

		AttributeInfo positions = *pos_info;

		for (int i = 0; i < positions.count; i++)
		{
			MeshVertex v = {};

			memcpy(v.pos, positions.start_addr + i * positions.stride, positions.elem_size);

			if (normal_info.has_value())
				memcpy(v.normals, normal_info->start_addr + i * normal_info->stride, normal_info->elem_size);

			if (tangent_info.has_value())
			{
				has_tangent = true; //Used below to know if need to auto compute
				memcpy(v.tangent, tangent_info->start_addr + i * tangent_info->stride, tangent_info->elem_size);
			}

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
		int indices_accessor_idx = m.primitives[primitive_idx].indices;
		const tinygltf::Accessor& accessor = model.accessors[indices_accessor_idx];
		const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
		auto data = model.buffers[bufferView.buffer].data.data() + bufferView.byteOffset + accessor.byteOffset;

		size_t elem_size = get_accessor_elem_size(accessor);
		size_t stride = bufferView.byteStride > 0 ? bufferView.byteStride : elem_size;
		for (int i = 0; i < accessor.count; i++)
		{
			//TODO: read the format correctly;
			uint32_t idx = 0;
			memcpy(&idx, data + i * stride, elem_size);
			//uint16_t* addr = (uint16_t*)(data + i * stride);
			out_mesh->indices.push_back(idx);
		}

	}

	if (!has_tangent)
	{
		ComputeTangents(out_mesh->vertices, out_mesh->indices);
	}



	const tinygltf::Material& material = model.materials[m.primitives[primitive_idx].material];


	const auto getImageSamplerIndex = [&] (int tex_idx) -> Mesh::ImageSamplerIndices {
		if (tex_idx < 0)
			return { -1, -1 };
		return { model.textures[tex_idx].source, model.textures[tex_idx].sampler };
	};


	out_mesh->material.baseColor = getImageSamplerIndex(material.pbrMetallicRoughness.baseColorTexture.index);
	out_mesh->material.mettalicRoughness = getImageSamplerIndex(material.pbrMetallicRoughness.metallicRoughnessTexture.index);
	out_mesh->material.normal = getImageSamplerIndex(material.normalTexture.index);
	out_mesh->material.occlusion = getImageSamplerIndex(material.occlusionTexture.index);
	out_mesh->material.emissive = getImageSamplerIndex(material.emissiveTexture.index);


	for (int i = 0; i < 4; i++)
	{
		out_mesh->material.pbrFactors.baseColorFactor[i] = material.pbrMetallicRoughness.baseColorFactor[i];
	}
	out_mesh->material.pbrFactors.metallicFactor = material.pbrMetallicRoughness.metallicFactor;
	out_mesh->material.pbrFactors.roughnessFactor = material.pbrMetallicRoughness.roughnessFactor;
	out_mesh->material.pbrFactors.occlusionStrength = material.occlusionTexture.strength;

	out_mesh->material.doubleSided = material.doubleSided;
	out_mesh->material.alphaCutoff = material.alphaCutoff;
	out_mesh->material.alphaMode = material.alphaMode;


	/*out_mesh->material = {
		.baseColor = material.pbrMetallicRoughness.baseColorTexture.index,
		.mettalicRoughness = material.pbrMetallicRoughness.metallicRoughnessTexture.index,
		.normal = material.normalTexture.index,
		.emissive = material.emissiveTexture.index,
		.occlusion = material.occlusionTexture.index,
	};*/
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
		loadGltfMesh(model, 0, 0, out_mesh);

	}

	return 0;
}

int loadGltf(const char* path, Scene* out_scene)
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
	if (model.scenes.size() == 0) {
		printf("This glTF file has no scenes\n");
		return -1;
	}


	for(auto img : model.images)
	{
		Texture t;
		t.height = img.height;
		t.width = img.width;
		t.channels = img.component;
		t.size = img.width * img.height * img.component;
		t.pixels = img.image;
		t.name = img.name.empty() ? img.uri : img.name;
		out_scene->textures.push_back(t);
	}

	for (auto sampl : model.samplers)
	{
		out_scene->samplers.push_back(SamplerInfo{
			.magFilter = sampl.magFilter,
			.minFilter = sampl.minFilter,
			.wrapS = sampl.wrapS,
			.wrapT = sampl.wrapT
		});
	}

	// Helper function to process nodes recursively
	std::function<Node(int, glm::mat4)> processNode = [&](int node_idx, glm::mat4 parent_transform) -> Node {
		const tinygltf::Node& gltf_node = model.nodes[node_idx];
		Node node;

		// Set node name
		node.name = gltf_node.name;

		// Calculate node's transformation matrix
		glm::mat4 local_transform = glm::mat4(1.0f);

		// Handle matrix if specified directly
		if (!gltf_node.matrix.empty()) {
			// glTF matrices are column-major
			for (int i = 0; i < 4; i++) {
				for (int j = 0; j < 4; j++) {
					//local_transform[i][j] = static_cast<float>(gltf_node.matrix[i + j * 4]);
					local_transform[i][j] = static_cast<float>(gltf_node.matrix[j + i * 4]);
				}
			}
		}
		else {
			// Handle TRS (Translation, Rotation, Scale) components
			// Translation
			if (!gltf_node.translation.empty()) {
				local_transform = glm::translate(
					local_transform,
					glm::vec3(
						static_cast<float>(gltf_node.translation[0]),
						static_cast<float>(gltf_node.translation[1]),
						static_cast<float>(gltf_node.translation[2])
					)
				);
			}

			// Rotation (quaternion)
			if (!gltf_node.rotation.empty()) {
				glm::quat q(
					static_cast<float>(gltf_node.rotation[3]), // w
					static_cast<float>(gltf_node.rotation[0]), // x
					static_cast<float>(gltf_node.rotation[1]), // y
					static_cast<float>(gltf_node.rotation[2])  // z
				);
				local_transform = local_transform * glm::mat4_cast(q);
			}

			// Scale
			if (!gltf_node.scale.empty()) {
				local_transform = glm::scale(
					local_transform,
					glm::vec3(
						static_cast<float>(gltf_node.scale[0]),
						static_cast<float>(gltf_node.scale[1]),
						static_cast<float>(gltf_node.scale[2])
					)
				);
			}
		}


		// Apply parent transform
		node.matrix = parent_transform * local_transform;

		// Process camera if present
		if (gltf_node.camera >= 0) {
			const tinygltf::Camera& gltf_camera = model.cameras[gltf_node.camera];
			Camera camera;

			if (gltf_camera.type == "perspective") {
				camera.fov = static_cast<float>(gltf_camera.perspective.yfov) * (180.0f / 3.14159f); // Convert to degrees
				camera.aspect = static_cast<float>(gltf_camera.perspective.aspectRatio);
				camera.znear = static_cast<float>(gltf_camera.perspective.znear);
				camera.zfar = static_cast<float>(gltf_camera.perspective.zfar);
			}
			else {
				// Default values for orthographic or if no specific type
				camera.fov = 45.0f;
				camera.aspect = 1.0f;
				camera.znear = 0.1f;
				camera.zfar = 100.0f;
			}

			node.data = camera;
		}
		// Process mesh if present
		else if (gltf_node.mesh >= 0) {
			const tinygltf::Mesh& gltf_mesh = model.meshes[gltf_node.mesh];
			
			std::vector<Mesh> primitives; //TODO : rename properly to remove Mesh/Primitives ambiguity
			primitives.reserve(gltf_mesh.primitives.size());
			
			for (int primitive_idx = 0; primitive_idx < gltf_mesh.primitives.size(); primitive_idx++)
			{
				Mesh mesh;
				loadGltfMesh(model, gltf_node.mesh, primitive_idx, &mesh);

				if(mesh.material.normal.texIdx >= 0)
					out_scene->textures[mesh.material.normal.texIdx].is_srgb = false;
				if(mesh.material.mettalicRoughness.texIdx >= 0)
					out_scene->textures[mesh.material.mettalicRoughness.texIdx].is_srgb = false;
				if(mesh.material.emissive.texIdx >= 0)
					out_scene->textures[mesh.material.emissive.texIdx].is_srgb = false;
				if(mesh.material.occlusion.texIdx >= 0)
					out_scene->textures[mesh.material.occlusion.texIdx].is_srgb = false;

				primitives.push_back(std::move(mesh));
			}

			node.data = std::move(primitives);
		}

		// Process children
		for (const auto& child_idx : gltf_node.children) {
			Node child_node = processNode(child_idx, node.matrix);
			node.children.push_back(new Node(child_node));
		}

		return node;
		};

	// Process root nodes
	tinygltf::Scene& s = model.scenes[0];
	for (const auto& node_idx : s.nodes) {
		Node root_node = processNode(node_idx, glm::mat4(1.0f));
		out_scene->nodes.push_back(root_node);
	}

	return 0;
}