#include "FileUtils.h"

#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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