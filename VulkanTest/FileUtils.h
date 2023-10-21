#pragma once

#include <vector>
#include <string>


struct Texture {
	uint32_t height;
	uint32_t width;
	int channels;
	unsigned char* pixels;
	size_t size;
};

std::vector<char> readFile(const std::string& filename);

Texture loadTexture(const char* path);
void freeTexturePixels(Texture* tex);