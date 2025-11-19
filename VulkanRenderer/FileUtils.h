#pragma once

#include <vector>
#include <string>
#include <variant>
#include <stdexcept>


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


struct Texture {
	uint32_t height;
	uint32_t width;
	int channels;
	size_t size;
	bool is_srgb = true;
	bool is_cubemap = false;

	using PixelData = std::variant<std::vector<unsigned char>, std::vector<float>>;
	PixelData pixels;
	std::string name;

	bool isHdr() const {
		return std::holds_alternative<std::vector<float>>(pixels);
	}

	template<typename T>
	const std::vector<T>& getPixels() const {
		if (!std::holds_alternative<std::vector<T>>(pixels))
			throw std::runtime_error("Texture is not this format");

		return std::get<std::vector<T>>(pixels);
	}

	const void* getRawPixels() const {
		return isHdr() ? (const void*)std::get<std::vector<float>>(pixels).data() : (const void*)std::get<std::vector<unsigned char>>(pixels).data();
	}
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


	std::vector<Texture> textures; /* DEPRECIATED */

	struct ImageSamplerIndices {
		int texIdx = -1;
		int samplerIdx = -1;
	};

	struct Material {
		ImageSamplerIndices baseColor;
		ImageSamplerIndices mettalicRoughness;
		ImageSamplerIndices normal;
		ImageSamplerIndices emissive;
		ImageSamplerIndices occlusion;

		struct PBRFactors {
			float baseColorFactor[4];
			float metallicFactor;
			float roughnessFactor;
			float occlusionStrength;
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

struct SamplerInfo {
	int magFilter;
	int minFilter;
	int wrapS;
	int wrapT;
};

struct Scene {
	std::vector<Node> nodes;
	std::vector<Texture> textures;
	std::vector<SamplerInfo> samplers;
};

std::vector<char> readFile(const std::string& filename);

Texture loadTexture(const char* path);
Texture loadCubemapTexture(const std::array<const char*, 6>& face_paths);
void freeTexturePixels(Texture* tex);

void loadObj(const char* path, Mesh* out_mesh);
int loadGltf(const char* path, Mesh* out_mesh);
int loadGltf(const char* path, Scene* out_scene);