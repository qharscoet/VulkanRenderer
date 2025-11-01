#pragma once

#include <memory>
#include <vector>

#include "Device.h"


using BufferHandle = std::shared_ptr<Buffer>;

class ResourceManager {
private:
	Device* m_device;


	std::array<Buffer, 512> m_buffers;
	size_t m_buffer_count = 0;

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

};