#pragma once

#include <vector>
#include <string>
#include <variant>




struct Texture {
	uint32_t height;
	uint32_t width;
	int channels;
	std::vector<unsigned char> pixels;
	size_t size;

	std::string name;
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

	struct Material {
		int baseColor;
		int mettalicRoughness;
		int normal;
		int emissive;
		int occlusion;
	} material;
};

//GLtf like structures
struct Camera
{
	float fov;
	float aspect;
	float znear;
	float zfar;
};

struct Node {
	std::string name;
	std::variant<Mesh, Camera> data;
	std::vector<Node*> children;
	//glm::mat4 transform; // transform
};

struct Scene {
	std::vector<Node> nodes;
};

std::vector<char> readFile(const std::string& filename);

Texture loadTexture(const char* path);
void freeTexturePixels(Texture* tex);

void loadObj(const char* path, Mesh* out_mesh);
int loadGltf(const char* path, Mesh* out_mesh);