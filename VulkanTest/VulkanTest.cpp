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

float zoom = 2.0f;
float rotation[3] = { 1.0f, 0.0f, 0.0f };
float translate[3] = { 0.0f, 0.0f, 0.0f };
bool auto_rot = false;

struct GLFW_state {
	bool pressed;
	double mousepress_x;
	double mousepress_y;
	int width;
	int height;
} glfw_state;

void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<Device*>(glfwGetWindowUserPointer(window));
	app->framebufferResized = true;

	glfw_state.width = width;
	glfw_state.height = height;

}



void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{

	if (button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		switch (action) {
		case GLFW_PRESS: glfw_state.pressed = true; break;
		case GLFW_RELEASE: glfw_state.pressed = false; break;
		default:break;
		}

		glfwGetCursorPos(window, &glfw_state.mousepress_x, &glfw_state.mousepress_y);
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	zoom -= yoffset * 0.1f;
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (glfw_state.pressed)
	{
		rotation[0] += (xpos - glfw_state.mousepress_x)/glfw_state.width;
		rotation[1] += (ypos - glfw_state.mousepress_y)/glfw_state.height;


		glfw_state.mousepress_x = xpos;
		glfw_state.mousepress_y = ypos;
	}

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

	RenderPass renderPass;


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

		glfwSetMouseButtonCallback(window, mouse_button_callback);
		glfwSetScrollCallback(window, scroll_callback);
		glfwSetCursorPosCallback(window, cursor_position_callback);

		glfw_state.width = WIDTH;
		glfw_state.height = HEIGHT;
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

		sampler = m_device.createTextureSampler(texture.mipLevels);

		m_device.updateDescriptorSets(texture.view, sampler);
	}


	void loadCube()
	{
		Mesh  cube_mesh, cube_test;
		loadGltf("assets/Cube/Cube.gltf", &cube_mesh);
		//loadGltf("assets/cube.gltf", &cube_mesh);

		vertexBuffer = m_device.createVertexBuffer(cube_mesh.vertices.size() * sizeof(cube_mesh.vertices[0]), (void*)cube_mesh.vertices.data());
		indexBuffer = m_device.createIndexBuffer(cube_mesh.indices.size() * sizeof(cube_mesh.indices[0]), (void*)cube_mesh.indices.data());
		m_device.setVertexBuffer(vertexBuffer);
		m_device.setIndexBuffer(indexBuffer);


		Texture tex = cube_mesh.textures.size() > 0 ? cube_mesh.textures[0] : loadTexture("assets/viking_room.png");

		texture = m_device.createTexture(tex);

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
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		PipelineDesc desc = {
			.type = PipelineType::Graphics,
			.vertexShader = "./shaders/basic.vert.spv",
			.pixelShader = "./shaders/basic.frag.spv",

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
			}
		};

		RenderPassDesc renderPassDesc = {
			.colorAttachement_count = 1,
			.hasDepth = true,
			.useMsaa = true
		};

		renderPass = m_device.createRenderPassAndPipeline(renderPassDesc, desc);
		
		m_device.setPipeline(renderPass.pipeline);
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
		};

		computePipeline = m_device.createComputePipeline(desc);
		computePipeline.descriptorPool = m_device.createDescriptorPool(desc.bindings.data(), desc.bindings.size());
		m_device.createComputeDescriptorSets(computePipeline);
	}
	void destroyPipeline() {
		m_device.destroyRenderPass(renderPass);
		m_device.destroyPipeline(computePipeline);
	}


	void updateUniformBuffer() {
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();

		float time =  1.0f + (auto_rot *  std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count());
		
		Dimensions dim = m_device.getExtent();

		UniformBufferObject ubo{};
		ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(translate[0], translate[1], translate[2]));
		ubo.model = glm::rotate(ubo.model, time * rotation[0] * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.model = glm::rotate(ubo.model, time * rotation[1] * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.model = glm::rotate(ubo.model, time * rotation[2] * glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		ubo.view = glm::lookAt(glm::vec3(zoom, zoom, zoom), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), dim.width / (float)dim.height, 0.1f, 20.0f);
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

			ImGui::SliderFloat("Zoom", &zoom, 0.0f, 10.0f);            // Edit 1 float using a slider from 0.0f to
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
			m_device.beginDraw();
			m_device.drawFrame();
			m_device.endDraw();
			//m_device.drawParticleFrame(computePipeline);
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
		loadCube();
		//initBuffers();
		//initTextures();
		//initParticlesBuffers();

		mainLoop();

		//cleanupParticles();
		cleanupTextures();
		cleanupBuffers();
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