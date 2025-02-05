#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX //Why msvc
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <optional>
#include <vector>
#include <array>
#include <variant>

#include "Pipeline.h"
#include "FileUtils.h"

typedef VkExtent2D Dimensions;

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct Buffer {
	VkBuffer buffer;
	VkDeviceMemory memory;
	void* mapped_memory;

	size_t size;
	size_t count;
	size_t stride;
};

struct ImageDesc {
	uint32_t width;
	uint32_t height;
	uint32_t mipLevels;
	VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT;
	VkFormat format;
	VkImageTiling tiling;
	VkImageUsageFlags usage_flags;
	VkMemoryPropertyFlags memory_properties;
	VkImageLayout initialLayout;
};

struct GpuImage {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	uint32_t mipLevels;
};


struct MeshPacket {
	Buffer vertexBuffer;
	Buffer indexBuffer;

	GpuImage texture;
	VkSampler sampler;
};

struct BarrierDesc {
	VkImageLayout oldLayout;
	VkImageLayout newLayout;
	uint32_t mipLevels;
};

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);



		return attributeDescriptions;
	}
};


struct Particle {
	glm::vec2 position;
	glm::vec2 velocity;
	glm::vec4 color;

	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Particle, position);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Particle, color);

		return attributeDescriptions;
	}
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct ParticleUBO {
	float deltaTime = 1.0f;
};

const uint32_t PARTICLE_COUNT = 512;

class Device {
private:
	GLFWwindow* window = nullptr;
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue computeQueue = VK_NULL_HANDLE;

	VkSurfaceKHR surface;
	VkQueue presentQueue;

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	std::vector<VkImageView> swapChainImageViews;

	VkRenderPass defaultRenderPass;


	RenderPass currentRenderPass;

	Buffer vertexBuffer;
	Buffer indexBuffer = { VK_NULL_HANDLE, VK_NULL_HANDLE, 0 };
	size_t index_count;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;

	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> computeDescriptorSets;

	GpuImage image;
	GpuImage depthBuffer;
	GpuImage colorTarget;

	std::vector<VkFramebuffer> swapChainFramebuffers;

	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkCommandBuffer> computeCommandBuffers;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	const int MAX_FRAMES_IN_FLIGHT = 2;
	std::vector<VkSemaphore>  imageAvailableSemaphores;
	std::vector<VkSemaphore>  renderFinishedSemaphores;
	std::vector<VkFence> computeInFlightFences;
	std::vector<VkSemaphore> computeFinishedSemaphores;
	std::vector<VkFence>  inFlightFences;
	uint32_t current_frame = 0;
	uint32_t current_framebuffer_idx = 0;

	VkDescriptorPool imgui_descriptorPool;
private:

	void initVulkan();
	void cleanupVulkan();

	void initImGui();
	void cleanupImGui();

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat findDepthFormat();
	bool hasStencilComponent(VkFormat format);

	VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDevice physicalDevice);

	bool isDeviceSuitable(VkPhysicalDevice device);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	void cleanupSwapChain();
	void recreateSwapChain();

	VkShaderModule createShaderModule(const std::vector<char>& code);

	void createCommandBuffer();
	void recordCommandBuffer(VkCommandBuffer commandBuffer);
	void recordComputeCommandBuffer(VkCommandBuffer commandBuffer, const Pipeline& computePipeline);

	void createInstance();
	void setupDebugMessenger();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();
	void createSwapChain();
	void createImageViews();

	void createDefaultRenderPass();
	void createColorResources();
	void createDepthBufferResources();
	void createFrameBuffers();
	void createCommandPool();
	void createUniformBuffers();
	void createSyncObjects();

public:
	bool framebufferResized = false; //public for now but may change

	Device();
	~Device();

	void init(GLFWwindow* window);
	void cleanup();

	void drawFrame();
	void beginDraw();
	void endDraw();
	void drawParticleFrame(const Pipeline& computePipeline);
	void waitIdle();

	void newImGuiFrame();


// Buffer and Texture stuff
private:

	class MyCommandBuffer;
	class ScopedCommandBuffer {
	private :
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		Device* parent_device = nullptr;
	public:
		friend class MyCommandBuffer;
		ScopedCommandBuffer(Device* device) 
			:parent_device(device)
			{ this->commandBuffer = device->beginSingleTimeCommands(); };
		ScopedCommandBuffer(ScopedCommandBuffer&& o) noexcept 
			: parent_device(std::exchange(o.parent_device, nullptr)),
			  commandBuffer(std::exchange(o.commandBuffer, VK_NULL_HANDLE)) {}
		~ScopedCommandBuffer() { if(commandBuffer) parent_device->endSingleTimeCommands(commandBuffer); commandBuffer = nullptr; };
		operator VkCommandBuffer() const { return commandBuffer; }
	};

	class MyCommandBuffer {
	private:
		std::variant<VkCommandBuffer, ScopedCommandBuffer> cb;
	public:
		MyCommandBuffer(VkCommandBuffer commandBuffer)
			:cb(commandBuffer) {};
		MyCommandBuffer(ScopedCommandBuffer&& o)
			:cb(std::move(o)) {};

		operator VkCommandBuffer() const
		{
			if (cb.index() == 0)
				return std::get<VkCommandBuffer>(cb);
			else
				return std::get<ScopedCommandBuffer>(cb).commandBuffer;
		}
	};

	VkCommandBuffer tmpCommandBuffer = VK_NULL_HANDLE;
	void setupCommandBuffer();
	void flushCommandBuffer();
	MyCommandBuffer getCommandBuffer();
	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& out_buffer, VkDeviceMemory& out_bufferMemory);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
	void generateMipmaps(VkImage image, VkFormat format, int32_t texWidth, int32_t texheight, uint32_t mipLevels);
	void createImage(ImageDesc desc, GpuImage& out_image);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
public:
	Buffer createLocalBuffer(size_t size,VkBufferUsageFlags usage,  void* src_data = nullptr);
	Buffer createVertexBuffer(size_t size, void* src_data = nullptr);
	Buffer createIndexBuffer(size_t size, void* src_data = nullptr);
	void destroyBuffer(Buffer buffer);
	void setVertexBuffer(Buffer vertexBuffer);
	void setIndexBuffer(Buffer indexBuffer);

	GpuImage createTexture(Texture tex);
	void destroyImage(GpuImage image);

	void destroySampler(VkSampler sampler) {
		vkDestroySampler(device, sampler, nullptr);
	}

	VkSampler createTextureSampler(uint32_t mipLevels);

	void updateUniformBuffer(void* data, size_t size);
	Dimensions getExtent() { return swapChainExtent;};

	void updateDescriptorSets(VkImageView imageView, VkSampler sampler);
	void updateDescriptorSet(VkImageView imageView, VkSampler sampler, uint32_t set_index);
	void updateComputeDescriptorSets(const std::vector<Buffer>& buffers);

	//Defined in Pipeline.cpp for now,, will probably make them independant at some point
private:

	VkRenderPass createRenderPass(RenderPassDesc desc);
	VkDescriptorSetLayout createDescriptorSetLayout(BindingDesc* bindings, size_t count);
	void createDescriptorSets(VkDescriptorSetLayout layout, VkDescriptorPool pool, VkDescriptorSet* out_sets);

public:

	Pipeline createPipeline(PipelineDesc desc);
	Pipeline createComputePipeline(PipelineDesc desc);
	RenderPass createRenderPassAndPipeline(RenderPassDesc renderPassDesc, PipelineDesc pipelineDesc);
	void setRenderPass(RenderPass renderPass);
	void drawPacket(MeshPacket packet);
	void destroyPipeline(Pipeline pipeline);
	void destroyRenderPass(RenderPass renderPass);

	void recordRenderPass(VkCommandBuffer commandBuffer);
	void recordRenderPass(VkCommandBuffer commandBuffer, RenderPass renderPass);

	VkDescriptorPool createDescriptorPool(BindingDesc* bindingDescs, size_t count);
	void setDescriptorPool(VkDescriptorPool pool);
	void createComputeDescriptorSets(const Pipeline& computePipeline);
};