#include "FileUtils.h"

#include <fstream>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
//#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include <tiny_obj_loader.h>

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

	return { (uint32_t)texWidth, (uint32_t)texHeight, texChannels, pixels, imageSize };
}


void freeTexturePixels(Texture* tex)
{
	stbi_image_free(tex->pixels);
	tex->pixels = nullptr;
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
		};
		uint32_t val;
	};

	std::unordered_map<uint32_t, uint32_t> uniqueVertices{};

	auto& attrib = reader.GetAttrib();

	for (const auto& shape : reader.GetShapes())
	{
		for (const auto& index : shape.mesh.indices) {
			MeshVertex vertex{};

			vertex.pos[0] = attrib.vertices[3 * index.vertex_index + 0];
			vertex.pos[1] = attrib.vertices[3 * index.vertex_index + 1];
			vertex.pos[2] = attrib.vertices[3 * index.vertex_index + 2];

			vertex.texCoord[0] = attrib.texcoords[2 * index.texcoord_index + 0];
			vertex.texCoord[1] = 1.0f - attrib.texcoords[2 * index.texcoord_index + 1];

			vertex.color[0] = 1.0f;
			vertex.color[1] = 1.0f;
			vertex.color[2] = 1.0f;

			idx_hash to_hash;
			to_hash.pos = index.vertex_index;
			to_hash.texCood = index.texcoord_index;

			if (uniqueVertices.count(to_hash.val) == 0)
			{
				uniqueVertices[to_hash.val] = out_mesh->vertices.size();
				out_mesh->vertices.push_back(vertex);
			}


			out_mesh->indices.push_back(uniqueVertices[to_hash.val]);

		}

	}
}