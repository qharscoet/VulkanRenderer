#include <iostream>
#include <stdexcept>

#include <chrono>


/* Optional TODOs :
	- Use the transfer queue for staging buffer
	- Use separate commandPool for copy buffer */



#include "Device.h"

void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<Device*>(glfwGetWindowUserPointer(window));
	app->framebufferResized = true;
}


class HelloTriangleApplication {

private:
	const uint32_t WIDTH = 800;
	const uint32_t HEIGHT = 600;

	Device m_device;
	GLFWwindow* window = nullptr;

	Buffer vertexBuffer;
	Buffer indexBuffer;

	const std::vector<Vertex> vertices = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
	};

	const std::vector<uint16_t> indices = {
		0, 1, 2, 2, 3, 0
	};

	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	void initBuffers() {
		vertexBuffer = m_device.createVertexBuffer(vertices.size()* sizeof(vertices[0]), (void*)vertices.data());
		indexBuffer = m_device.createIndexBuffer(indices.size()* sizeof(indices[0]), (void*)indices.data());
		m_device.setVertexBuffer(vertexBuffer);
		m_device.setIndexBuffer(indexBuffer);
	}

	void cleanupBuffers() {
		m_device.destroyBuffer(vertexBuffer);
		m_device.destroyBuffer(indexBuffer);
	}


	void cleanupWindow() {
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void updateUniformBuffer() {
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();

		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		
		Dimensions dim = m_device.getExtent();

		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), dim.width / (float)dim.height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		m_device.updateUniformBuffer(ubo);
		//memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			updateUniformBuffer();
			m_device.drawFrame();
		}

		m_device.waitIdle();
	}

public:
	void run() {
		initWindow();
		m_device.init(window);
		initBuffers();

		mainLoop();

		cleanupBuffers();
		m_device.cleanup();
		cleanupWindow();
	}
};

int main() {
	HelloTriangleApplication app;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}