#pragma once

#include <memory>
#include <vector>

#include "Device.h"

enum class DeviceTextureType {
	RWTexture,
	RenderTarget,
	DepthTarget
};


enum class DeviceBufferType {
	Vertex,
	Index,
	Uniform
};

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

	std::array<Sampler, 256> m_samplers;
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

	BufferHandle createUniformBuffer(size_t size, void* src_data = nullptr) {
		m_buffers[m_buffer_count++] = m_device->createUniformBuffer(size, src_data);

		Buffer* buf = &m_buffers[m_buffer_count - 1];

		return BufferHandle(buf, [this](Buffer* buf) {
			m_device->destroyBuffer(*buf);
			});
	}

	template<DeviceBufferType type, class... Args>
	BufferHandle createBuffer(size_t size, void* src_data = nullptr)
	{
		Buffer& buf = m_buffers[m_buffer_count++];
		// Forward the arguments to the appropriate create function of Device based on BufferType 
		if constexpr (type == DeviceBufferType::Vertex)
			buf = m_device->createVertexBuffer(size, src_data);
		else if constexpr (type == DeviceBufferType::Index)
			buf = m_device->createIndexBuffer(size, src_data);
		else if constexpr (type == DeviceBufferType::Uniform)
			buf = m_device->createUniformBuffer(size, src_data);
		else
			static_assert(false, "Unsupported BufferType");

		return BufferHandle(&buf, [this](Buffer* buf) {
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
		m_device->createRWTexture(m_textures[m_texture_count++], w, h,format, is_cubemap, true, allocateMips);

		return GpuImageHandle(&m_textures[m_texture_count - 1], [this](GpuImage* img) {
			m_device->destroyImage(*img);
			});
	}


	template<DeviceTextureType type, class... Args>
	GpuImageHandle createTexture(Args&&... args)
	{
		GpuImage& img = m_textures[m_texture_count++];

		// Forward the arguments to the appropriate create function of Device based on TextureType 
		if constexpr (type == DeviceTextureType::RWTexture)
			m_device->createRWTexture(img, std::forward<decltype(args)>(args)...);
		else if constexpr (type == DeviceTextureType::RenderTarget)
			m_device->createRenderTarget(img, std::forward<decltype(args)>(args)...);
		else if constexpr (type == DeviceTextureType::DepthTarget)
			m_device->createDepthTarget(img, std::forward<decltype(args)>(args)...);
		else
			static_assert(false, "Unsupported TextureType");

		return GpuImageHandle(&img, [this](GpuImage* img) {
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