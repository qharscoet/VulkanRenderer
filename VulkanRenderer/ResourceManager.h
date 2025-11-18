#pragma once

#include <memory>
#include <vector>

#include "Device.h"


using BufferHandle = std::shared_ptr<Buffer>;
using GpuImageHandle = std::shared_ptr<GpuImage>;

using Sampler = VkSampler;
using SamplerHandle = std::shared_ptr<Sampler>;

class ResourceManager {
private:
	Device* m_device;


	std::array<Buffer, 512> m_buffers;
	size_t m_buffer_count = 0;

	std::array<GpuImage, 512> m_textures;
	size_t m_texture_count = 0;

	std::array<Sampler, 32> m_samplers;
	size_t m_sampler_count = 0;

public:
	ResourceManager(Device* device) : m_device(device) { }
	~ResourceManager() {}

	BufferHandle createVertexBuffer(size_t size, void* src_data = nullptr) {
		m_buffers[m_buffer_count++] = m_device->createVertexBuffer(size, src_data);

		Buffer* buf = &m_buffers[m_buffer_count - 1];

		return BufferHandle(buf, [this](Buffer* buf) {
			m_device->destroyBuffer(*buf);
			});
	}

	BufferHandle createIndexBuffer(size_t size, void* src_data = nullptr) {
		m_buffers[m_buffer_count++] = m_device->createIndexBuffer(size, src_data);

		Buffer* buf = &m_buffers[m_buffer_count - 1];

		return BufferHandle(buf, [this](Buffer* buf) {
			m_device->destroyBuffer(*buf);
		});
	}


	GpuImageHandle createTexture(const Texture& texture) {
		m_textures[m_texture_count++] = m_device->createTexture(texture);
		GpuImage* img = &m_textures[m_texture_count - 1];

		return GpuImageHandle(img, [this](GpuImage* img) {
			m_device->destroyImage(*img);
			});
	}

	GpuImageHandle createRWTexture(uint32_t w, uint32_t h, ImageFormat format, bool is_cubemap, bool allocateMips = false) {
		m_device->createRWTexture(w, h, m_textures[m_texture_count++], format, is_cubemap, true, allocateMips);

		return GpuImageHandle(&m_textures[m_texture_count - 1], [this](GpuImage* img) {
			m_device->destroyImage(*img);
			});
	}


	SamplerHandle createSampler(SamplerDesc desc)
	{
		m_samplers[m_sampler_count++] = m_device->createTextureSampler(desc);

		return SamplerHandle(&m_samplers[m_sampler_count - 1], [this](Sampler* sampl) {
			m_device->destroySampler(*sampl);
		});
	}
};