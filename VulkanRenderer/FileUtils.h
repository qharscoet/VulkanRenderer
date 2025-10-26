#pragma once

#include <vector>
#include <string>
#include <variant>


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


struct Texture {
	uint32_t height;
	uint32_t width;
	int channels;
	std::vector<unsigned char> pixels;
	size_t size;
	bool is_srgb = true;

	std::string name;
};

struct MeshVertex
{
	float pos[3];
	float normals[3];
	float color[3];
	float texCoord[2];
	float tangent[4];
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

		struct PBRFactors {
			float baseColorFactor[4];
			float metallicFactor;
			float roughnessFactor;
		} pbrFactors;
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
	std::variant<std::vector<Mesh>, Camera> data;
	std::vector<Node*> children;
	glm::mat4 matrix; // transform
};

struct Scene {
	std::vector<Node> nodes;
};

std::vector<char> readFile(const std::string& filename);

Texture loadTexture(const char* path);
void freeTexturePixels(Texture* tex);

void loadObj(const char* path, Mesh* out_mesh);
int loadGltf(const char* path, Mesh* out_mesh);
int loadGltf(const char* path, Scene* out_scene);