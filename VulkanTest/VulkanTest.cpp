#include <iostream>
#include <stdexcept>

#include <chrono>
#include <random>

#include "FileUtils.h"

/* Optional TODOs :
	- Use the transfer queue for staging buffer
	- Use separate commandPool for copy buffer */



#include "Device.h"

#include "imgui.h"

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

	GpuImage texture;
	//VkImageView textureImageView;
	VkSampler sampler;

	Pipeline pipeline;
	VkDescriptorPool descriptorPool;

	std::vector<Buffer> particleStorageBuffers;
	double lastTime;
	double lastFrameTime;

	Pipeline computePipeline;
	VkDescriptorPool computeDescriptorPool;

	const std::vector<MeshVertex> vertices = {
		{.pos = {-0.5f, -0.5f, 0.0f},	.color = {1.0f, 0.0f, 0.0f},	.texCoord = {0.0f, 0.0f} },
		{.pos = {0.5f, -0.5f, 0.0f},	.color = {0.0f, 1.0f, 0.0f},	.texCoord = {1.0f, 0.0f} },
		{.pos = {0.5f, 0.5f, 0.0f},		.color = {0.0f, 0.0f, 1.0f},	.texCoord = {1.0f, 1.0f} },
		{.pos = {-0.5f, 0.5f, 0.0f},	.color = {1.0f, 1.0f, 1.0f},	.texCoord = {0.0f, 1.0f} },

		{.pos = {-0.5f, -0.5f, -0.5f},	.color = {1.0f, 0.0f, 0.0f},	.texCoord = {0.0f, 0.0f} },
		{.pos = {0.5f, -0.5f, -0.5f},	.color = {0.0f, 1.0f, 0.0f},	.texCoord = {1.0f, 0.0f} },
		{.pos = {0.5f, 0.5f, -0.5f},	.color = {0.0f, 0.0f, 1.0f},	.texCoord = {1.0f, 1.0f} },
		{.pos = {-0.5f, 0.5f, -0.5f},	.color = {1.0f, 1.0f, 1.0f},	.texCoord = {0.0f, 1.0f} },
	};

	const std::vector<uint32_t> indices = {
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4
	};

	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

		glfwSetWindowUserPointer(window, &m_device);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	void cleanupWindow() {
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void initBuffers() {
		vertexBuffer = m_device.createVertexBuffer(vertices.size() * sizeof(vertices[0]), (void*)vertices.data());
		indexBuffer = m_device.createIndexBuffer(indices.size() * sizeof(indices[0]), (void*)indices.data());
		m_device.setVertexBuffer(vertexBuffer);
		m_device.setIndexBuffer(indexBuffer);
	}

	void initParticlesBuffers() {
		particleStorageBuffers.resize(2);
		// Initialize particles
		std::default_random_engine rndEngine((unsigned)time(nullptr));
		std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

		// Initial particle positions on a circle
		std::vector<Particle> particles(PARTICLE_COUNT);
		for (auto& particle : particles) {
			float r = 0.25f * sqrt(rndDist(rndEngine));
			float theta = rndDist(rndEngine) * 2 * 3.14159265358979323846;
			float x = r * cos(theta) * HEIGHT / WIDTH;
			float y = r * sin(theta);
			particle.position = glm::vec2(x, y);
			particle.velocity = glm::normalize(glm::vec2(x, y)) * 0.0025f;
			particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
		}

		VkDeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

		//TODO change this to not allocate the staging buffer 3 times
		for (size_t i = 0; i < 2; i++) {
			particleStorageBuffers[i] = m_device.createLocalBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, particles.data());
			particleStorageBuffers[i].size = bufferSize;
			particleStorageBuffers[i].stride = sizeof(Particle);
			particleStorageBuffers[i].count = bufferSize / sizeof(Particle);
		}

		m_device.updateComputeDescriptorSets(particleStorageBuffers);
		m_device.setVertexBuffer(particleStorageBuffers[0]);

	}

	void cleanupParticles()
	{
		for (int i = 0; i < 2; i++)
		{
			m_device.destroyBuffer(particleStorageBuffers[i]);
		}
	}

	void loadViking() {
		Mesh  viking_mesh;
		loadObj("assets/viking_room.obj", &viking_mesh);

		vertexBuffer = m_device.createVertexBuffer(viking_mesh.vertices.size() * sizeof(viking_mesh.vertices[0]), (void*)viking_mesh.vertices.data());
		indexBuffer = m_device.createIndexBuffer(viking_mesh.indices.size() * sizeof(viking_mesh.indices[0]), (void*)viking_mesh.indices.data());
		m_device.setVertexBuffer(vertexBuffer);
		m_device.setIndexBuffer(indexBuffer);


		Texture tex = loadTexture("assets/viking_room.png");

		texture = m_device.createTexture(tex);
		freeTexturePixels(&tex);

		sampler = m_device.createTextureSampler(texture.mipLevels);

		m_device.updateDescriptorSets(texture.view, sampler);
	}


	void cleanupBuffers() {
		m_device.destroyBuffer(vertexBuffer);
		m_device.destroyBuffer(indexBuffer);
	}


	//TODO update the API to make param as out rather than return ?
	void initTextures() {
		Texture tex = loadTexture("assets/texture.jpg");

		texture = m_device.createTexture(tex);
		freeTexturePixels(&tex);

		sampler = m_device.createTextureSampler(texture.mipLevels);

		m_device.updateDescriptorSets(texture.view, sampler);
	}

	void cleanupTextures() {
		m_device.destroyImage(texture);
		//m_device.destoryImageView(textureImageView);
		m_device.destroySampler(sampler);

	}


	void initPipeline() {
		auto attributeDescriptions = Particle::getAttributeDescriptions();

		PipelineDesc desc = {
			.type = PipelineType::Graphics,
			.vertexShader = "./shaders/particles.vert.spv",
			.pixelShader = "./shaders/particles.frag.spv",

			.attributeDescriptions = attributeDescriptions.data(),
			.attributeDescriptionsCount = attributeDescriptions.size(),

			.blendMode = BlendMode::Opaque,
			.bindings = {
				{
					.slot = 0,
					.type = BindingType::UBO,
					.stageFlags = e_Vertex,
				},
				{
					.slot = 1,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				}
			},

			.useDefaultRenderPass = false,
			.colorAttachment = 1,
			.hasDepth = false,
			.useMsaa = false
		};

		pipeline = m_device.createPipeline(desc);
		descriptorPool = m_device.createDescriptorPool(desc.bindings.data(), desc.bindings.size());
		
		m_device.setPipeline(pipeline);
		m_device.setDescriptorPool(descriptorPool);
		m_device.createDescriptorSets();

	
	}

	void initComputePipeline() 
	{
		PipelineDesc desc = {
			.type = PipelineType::Compute,
			.computeShader = "./shaders/particles.comp.spv",

			.bindings = {
				{
					.slot = 0,
					.type = BindingType::UBO,
					.stageFlags = e_Compute,
				},
				{
					.slot = 1,
					.type = BindingType::StorageBuffer,
					.stageFlags = e_Compute,
				},
				{
					.slot = 2,
					.type = BindingType::StorageBuffer,
					.stageFlags = e_Compute,
				}
			},

			.useDefaultRenderPass = false,
		};

		computePipeline = m_device.createComputePipeline(desc);
		computePipeline.descriptorPool = m_device.createDescriptorPool(desc.bindings.data(), desc.bindings.size());
		m_device.createComputeDescriptorSets(computePipeline);
	}
	void destroyPipeline() {
		m_device.destroyPipeline(pipeline);
		m_device.destroyPipeline(computePipeline);
	}

	float zoom = 2.0f;
	float rotation[3] = { 1.0f, 0.0f, 0.0f };
	float translate[3] = { 0.0f, 0.0f, 0.0f };
	bool auto_rot = false;


	void updateUniformBuffer() {
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();

		float time = auto_rot *  std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		
		Dimensions dim = m_device.getExtent();

		UniformBufferObject ubo{};
		ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(translate[0], translate[1], translate[2]));
		ubo.model = glm::rotate(ubo.model, time * rotation[0] * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.model = glm::rotate(ubo.model, time * rotation[1] * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.model = glm::rotate(ubo.model, time * rotation[2] * glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		ubo.view = glm::lookAt(glm::vec3(zoom, zoom, zoom), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), dim.width / (float)dim.height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		m_device.updateUniformBuffer(&ubo, sizeof(UniformBufferObject));
		//memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
	}

	void updateParticleUniformBuffer()
	{
		double currentTime = glfwGetTime();
		 lastFrameTime = (currentTime - lastTime) * 1000.0;
		 lastTime = currentTime;

		ParticleUBO ubo{};
		ubo.deltaTime = lastFrameTime * 2.0f;
		m_device.updateUniformBuffer(&ubo, sizeof(ParticleUBO));
	}

	void drawImGui()
	{
		ImGuiIO& io = ImGui::GetIO();

		static bool show_demo_window = false;
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		{

			ImGui::Begin("Params");                          // Create a window called "Hello, world!" and append into it.

			ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

			ImGui::SliderFloat("Zoom", &zoom, 0.0f, 2.0f);            // Edit 1 float using a slider from 0.0f to
			ImGui::SliderFloat3("Rot", rotation, 0.0f, 4.0f);            // Edit 1 float using a slider from 0.0f to
			ImGui::SliderFloat3("Translate", translate, 0.0f, 1.0f);          // Edit 1 float using a slider from 0.0f to
			ImGui::Checkbox("Auto Rotate", &auto_rot);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			ImGui::End();
		}
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			m_device.newImGuiFrame();
			drawImGui();
			updateUniformBuffer();
			m_device.drawParticleFrame(computePipeline);
		}

		m_device.waitIdle();
	}

public:
	void run() {
		initWindow();
		m_device.init(window);
		initPipeline();
		initComputePipeline();
		//loadViking();
		//initBuffers();
		//initTextures();
		initParticlesBuffers();

		mainLoop();

		cleanupParticles();
		//cleanupTextures();
		//cleanupBuffers();
		destroyPipeline();
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