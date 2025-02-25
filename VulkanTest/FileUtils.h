#pragma once

#include <vector>
#include <string>




struct Texture {
	uint32_t height;
	uint32_t width;
	int channels;
	std::vector<unsigned char> pixels;
	size_t size;
};

struct MeshVertex
{
	float pos[3];
	float normals[3];
	float color[3];
	float texCoord[2];
};

struct Mesh {
	std::vector<MeshVertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Texture> textures;
};

struct Scene {

};

std::vector<char> readFile(const std::string& filename);

Texture loadTexture(const char* path);
void freeTexturePixels(Texture* tex);

void loadObj(const char* path, Mesh* out_mesh);
int loadGltf(const char* path, Mesh* out_mesh);