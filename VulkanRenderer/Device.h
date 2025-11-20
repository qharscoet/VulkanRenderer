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
#include <memory>

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
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	void* mapped_memory = nullptr;

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
	bool is_cubemap = false;
};

enum class ImageFormat {
	Undefined,
	RGB8,
	RGB8_Unorm,
	RGBA8,
	RGBA8_Unorm,
	RGB_Float,
	RGBA_Float,
	RG16_Float,
};

struct GpuImage {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	std::vector<VkImageView> writeViews;
	ImageFormat format;
	uint32_t mipLevels = 1;
	uint32_t layerCount = 1;
	uint32_t width;
	uint32_t height;
};

enum class FilterMode {
	Linear, Nearest, Nearest_MipNearest, Nearest_MipLinear, Linear_MipNearest, Linear_MipLinear
};

enum class WrapMode {
	Clamp, Repeat, MirroredRepeat
};

struct SamplerDesc {
	FilterMode magFilter = FilterMode::Linear;
	FilterMode minFilter = FilterMode::Linear;
	WrapMode wrapS = WrapMode::Repeat;
	WrapMode wrapT = WrapMode::Repeat;
};


using BufferHandle = std::shared_ptr<Buffer>;
using GpuImageHandle = std::shared_ptr<GpuImage>;

using Sampler = VkSampler;
using SamplerHandle = std::shared_ptr<Sampler>;

struct ImageBindInfo {
	VkImageView imageview;
	VkSampler sampl = VK_NULL_HANDLE;
};

//using ImageBindInfo = std::variant<ImageSamplerBindInfo, VkImageView>;

struct MeshPacket {
	BufferHandle vertexBuffer;
	BufferHandle indexBuffer;

	std::vector<GpuImageHandle> textures;
	std::vector<SamplerHandle> samplers;

	//struct Transform {
	//	float translation[3];
	//	float rotation[3];
	//	float scale[3];
	//} transform;

	glm::mat4 transform = glm::mat4(1.0);

	struct PushConstantsData {
		glm::mat4 model;
	};

	struct ImageSamplerIndices {
		int texIdx = -1;
		int samplerIdx = -1;
	};

	enum TextureType {
		BaseColor, MetallicRoughness, Normal, Emissive, Occlusion,
		Nb
	};

	struct MaterialData {
		ImageSamplerIndices texturesIdx[TextureType::Nb];

		struct AlphaCoverage {
			enum AlphaMode {
				Opaque,
				Mask,
				Blend
			} alphaMode = Opaque;
			
			float cutoff = 0.5f;
		} alphaCoverage;

		struct PBRFactors {
			float baseColorFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			float metallicFactor = 1.0f;
			float roughnessFactor = 1.0f;
			float occlusionStrength = 1.0f;
		} pbrFactors;

		float getAlphaCutoff() const {
			float alphaCutoff;
			if (alphaCoverage.alphaMode == AlphaCoverage::AlphaMode::Mask)
				alphaCutoff = alphaCoverage.cutoff;
			else
				alphaCutoff = 0.0f;
			return alphaCutoff;
		}

	} materialData;

	std::string name;

	const ImageBindInfo getTextureBindInfo(const TextureType type, const GpuImageHandle defaultTexture, const SamplerHandle defaultSampl) const
	{
		const ImageSamplerIndices indices = materialData.texturesIdx[type];
		const GpuImageHandle& image = indices.texIdx >= 0 ? textures[indices.texIdx] : defaultTexture;
		const SamplerHandle& sampler = indices.samplerIdx >= 0 ? samplers[indices.samplerIdx] : defaultSampl;

		return { image->view, *sampler };
	}
};

struct Vertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 texCoord;
	glm::vec4 tangent;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, normal);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, color);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(Vertex, texCoord);
		
		attributeDescriptions[4].binding = 0;
		attributeDescriptions[4].location = 4;
		attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[4].offset = offsetof(Vertex, tangent);



		return attributeDescriptions;
	}


	static std::vector<Vertex> getCubeVertices()
	{
		return {
			// Front face (Z+)
			{{-0.5f, -0.5f,  0.5f}, {0.0f,  0.0f,  1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{ 0.5f, -0.5f,  0.5f}, {0.0f,  0.0f,  1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {0.0f,  0.0f,  1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{-0.5f,  0.5f,  0.5f}, {0.0f,  0.0f,  1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},

			// Back face (Z-)
			{{ 0.5f, -0.5f, -0.5f}, {0.0f,  0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
			{{-0.5f, -0.5f, -0.5f}, {0.0f,  0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
			{{-0.5f,  0.5f, -0.5f}, {0.0f,  0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
			{{ 0.5f,  0.5f, -0.5f}, {0.0f,  0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},

			// Left face (X-)
			{{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
			{{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
			{{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
			{{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},

			// Right face (X+)
			{{ 0.5f, -0.5f,  0.5f}, {1.0f,  0.0f,  0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
			{{ 0.5f, -0.5f, -0.5f}, {1.0f,  0.0f,  0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
			{{ 0.5f,  0.5f, -0.5f}, {1.0f,  0.0f,  0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {1.0f,  0.0f,  0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
		};

	}
	static std::vector<uint32_t> getCubeIndices() {
		return {
			0, 1, 2, 2, 3, 0, // Front face
			4, 5, 6, 6, 7, 4, // Back face
			8, 9, 10, 10, 11, 8, // Left face
			12, 13, 14, 14, 15, 12, // Right face
			16, 17, 18, 18, 19, 16, // Bottom face
			20, 21, 22, 22, 23, 20 // Top face
		};
	}

	static std::vector<Vertex> getConeVertices()
	{
		return {
			// Tip of the cone (apex)
			{{0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},

			// Base vertices (forming a circle in the XZ plane at y = 0)
			{{0.5f, 0.0f, 0.0f},  {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}},
			{{0.25f, 0.0f, 0.433f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.75f, 0.75f}, {-0.866f, 0.0f, 0.5f, 1.0f}},
			{{-0.25f, 0.0f, 0.433f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.5f, 1.0f}, {-0.866f, 0.0f, -0.5f, 1.0f}},
			{{-0.5f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.25f, 0.75f}, {0.0f, 0.0f, -1.0f, 1.0f}},
			{{-0.25f, 0.0f, -0.433f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.5f}, {0.866f, 0.0f, -0.5f, 1.0f}},
			{{0.25f, 0.0f, -0.433f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.25f, 0.25f}, {0.866f, 0.0f, 0.5f, 1.0f}},
			{{0.5f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
		};
	}
	static std::vector<uint32_t> getConeIndices() {
		return {
			// Side triangles (connect the tip to each base vertex)
			0, 1, 2,
			0, 2, 3,
			0, 3, 4,
			0, 4, 5,
			0, 5, 6,
			0, 6, 7,

			// Base triangles (fan around the center)
			1, 3, 2,
			1, 4, 3,
			1, 5, 4,
			1, 6, 5,
			1, 7, 6
		};
	}

	static std::vector<Vertex> generateSphereVertices() {
		const unsigned int X_SEGMENTS = 64;
		const unsigned int Y_SEGMENTS = 64;
		const float PI = 3.14159265359f;
		std::vector<Vertex> vertices;
		for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
			for (unsigned int y = 0; y <= Y_SEGMENTS; ++y) {
				float xSegment = (float)x / (float)X_SEGMENTS;
				float ySegment = (float)y / (float)Y_SEGMENTS;
				float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
				float yPos = std::cos(ySegment * PI);
				float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

				Vertex vertex;
				vertex.pos = glm::vec3(xPos, yPos, zPos);
				vertex.normal = glm::vec3(xPos, yPos, zPos);
				vertex.texCoord = glm::vec2(xSegment, ySegment);
				vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
				vertex.tangent = glm::vec4(0.0f); // Will be computed later
				vertices.push_back(vertex);
			}
		}
		return vertices;
	}


	static std::vector<uint32_t> generateSphereIndices() {
		const unsigned int X_SEGMENTS = 64;
		const unsigned int Y_SEGMENTS = 64;
		std::vector<uint32_t> indices;
		for (unsigned int y = 0; y < Y_SEGMENTS; ++y) {
			for (unsigned int x = 0; x < X_SEGMENTS; ++x) {

				indices.push_back(y * (X_SEGMENTS + 1) + x);
				indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);

				indices.push_back((y + 1) * (X_SEGMENTS + 1) + x + 1);
				indices.push_back(y * (X_SEGMENTS + 1) + x);
				indices.push_back((y + 1) * (X_SEGMENTS + 1) + x + 1);
				indices.push_back(y * (X_SEGMENTS + 1) + x + 1);
			}
		}

		return indices;
	}

	static void ComputeTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
		// Temporary containers to store tangents
		std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.0f));

		// Iterate over each triangle
		for (size_t i = 0; i < indices.size(); i += 3) {
			// Get triangle indices
			uint32_t i0 = indices[i];
			uint32_t i1 = indices[i + 1];
			uint32_t i2 = indices[i + 2];

			// Get triangle vertices
			Vertex& v0 = vertices[i0];
			Vertex& v1 = vertices[i1];
			Vertex& v2 = vertices[i2];

			// Compute edge vectors
			glm::vec3 edge1 = v1.pos - v0.pos;
			glm::vec3 edge2 = v2.pos - v0.pos;

			// Compute delta UVs
			glm::vec2 deltaUV1 = v1.texCoord - v0.texCoord;
			glm::vec2 deltaUV2 = v2.texCoord - v0.texCoord;

			// Compute tangent
			float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
			glm::vec3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);

			// Accumulate tangents
			tangents[i0] += tangent;
			tangents[i1] += tangent;
			tangents[i2] += tangent;
		}

		// Normalize and store tangents
		for (size_t i = 0; i < vertices.size(); ++i) {
			glm::vec3 T = glm::normalize(tangents[i]);

			// Ensure the tangent is orthogonal to the normal
			T = glm::normalize(T - vertices[i].normal * glm::dot(vertices[i].normal, T));

			// Compute handedness (w component)
			glm::vec3 bitangent = glm::cross(vertices[i].normal, T);
			float handedness = (glm::dot(bitangent, T) < 0.0f) ? -1.0f : 1.0f;

			// Store tangent in vertex
			vertices[i].tangent[0] = T.x;
			vertices[i].tangent[1] = T.y;
			vertices[i].tangent[2] = T.z;
			vertices[i].tangent[3] = handedness;
		}
	}


};


struct Particle {
	glm::vec2 position;
	glm::vec2 velocity;
	glm::vec4 color;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Particle);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

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
	glm::mat4 view;
	glm::mat4 proj;
};

struct ParticleUBO {
	float deltaTime = 1.0f;
};

const uint32_t PARTICLE_COUNT = 512;

struct DeviceOptions {
	bool usesMsaa;
};

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
	bool usesMsaa;
	std::optional<bool> nextUsesMsaa;


	Pipeline* currentPipeline;


	std::vector<Buffer> uniformBuffers;

	std::vector<Buffer> computeUniformBuffers;
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
	void refreshImGui();

	PFN_vkCmdBeginDebugUtilsLabelEXT CmdBeginDebugUtilsLabel = nullptr;
	PFN_vkCmdEndDebugUtilsLabelEXT CmdEndDebugUtilsLabel = nullptr;
	PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectName = nullptr;

	void PushCmdLabel(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* markerInfo) {
		if(CmdBeginDebugUtilsLabel)
			CmdBeginDebugUtilsLabel(commandBuffer, markerInfo);
	}

	void EndCmdLabel(VkCommandBuffer commandBuffer) {
		if (CmdEndDebugUtilsLabel)
			CmdEndDebugUtilsLabel(commandBuffer);
	}

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat findDepthFormat();
	bool hasStencilComponent(VkFormat format);

	VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDevice physicalDevice);

	uint32_t rateDeviceSuitability(VkPhysicalDevice device);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	void cleanupSwapChain();
	void recreateSwapChain();

	VkShaderModule createShaderModule(const std::vector<char>& code);

	void createCommandBuffer();
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

	VkSampleCountFlagBits getMsaaSamples() { return this->usesMsaa ? msaaSamples : VK_SAMPLE_COUNT_1_BIT; };


private:
	bool hasRecorededCompute = false;
	bool skipDraw = false;

public:
	bool framebufferResized = false; //public for now but may change

	Device();
	~Device();

	void init(GLFWwindow* window, DeviceOptions options);
	void cleanup();

	void beginDraw();
	void endDraw();
	
	void waitIdle();


	void SetRenderPassName(VkRenderPass renderpass, const char* name);
	void SetShaderModuleName(VkShaderModule module, const char* name);
	void SetImageName(VkImage image, const char* name);
	void SetBufferName(VkBuffer buffer, const char* name);

	uint32_t getCurrentFrame() { return current_frame; }
	uint32_t getMaxFramesInFlight() { return MAX_FRAMES_IN_FLIGHT; }

	void newImGuiFrame();
	void setUsesMsaa(bool usesMsaa) {
		if (usesMsaa != this->usesMsaa)
		{
			//this->usesMsaa = usesMsaa;
			this->nextUsesMsaa = usesMsaa;
			this->framebufferResized = true; //This will trigger swapchain recreation
		}

	};


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
	
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layerCount, VkCommandBuffer cb = VK_NULL_HANDLE );
	void generateMipmaps(VkImage image, VkFormat format, int32_t texWidth, int32_t texheight, uint32_t mipLevels, uint32_t layerCount);
	void createImage(ImageDesc desc, GpuImage& out_image);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t baseMip, uint32_t mipCount, bool isCubemap, bool write = false);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, size_t layerSize, uint32_t layerCount);
public:
	Buffer createLocalBuffer(size_t size,VkBufferUsageFlags usage,  void* src_data = nullptr);
	Buffer createVertexBuffer(size_t size, void* src_data = nullptr);
	Buffer createIndexBuffer(size_t size, void* src_data = nullptr);
	Buffer createUniformBuffer(size_t size, void* src_data = nullptr);
	void destroyBuffer(Buffer& buffer);

	GpuImage createTexture(Texture tex);
	void createRWTexture(uint32_t width, uint32_t height, GpuImage& out_image, ImageFormat format, bool is_cubemap, bool sampled = false, bool allocateMips = false);
	void createRenderTarget(uint32_t width, uint32_t height, GpuImage& out_image, bool msaa, bool sampled = false);
	void createDepthTarget(uint32_t width, uint32_t height, GpuImage& out_image, bool msaa);
	void destroyImage(GpuImage image);

	void destroySampler(VkSampler sampler) {
		vkDestroySampler(device, sampler, nullptr);
	}

	VkSampler createTextureSampler(SamplerDesc desc);

	void updateUniformBuffer(void* data, size_t size);
	void updateComputeUniformBuffer(void* data, size_t size);
	Dimensions getExtent() { return swapChainExtent;};
	const Buffer& getCurrentUniformBuffer() { return uniformBuffers[current_frame]; };
	const Buffer& getCurrentComputeUniformBuffer() { return computeUniformBuffers[current_frame]; };

	VkDescriptorBufferInfo& getDescriptorBufferInfo(const Buffer& buffer);
	VkDescriptorImageInfo& getDescriptorImageInfo(const ImageBindInfo image);

	void updateDescriptorSet(const std::vector<BindingDesc>& bindings, std::vector<VkDescriptorImageInfo>& images, std::vector<VkDescriptorBufferInfo>& buffers, VkDescriptorSet set);
	void updateComputeDescriptorSets(const std::vector<Buffer>& buffers);


	//Defined in Pipeline.cpp for now,, will probably make them independant at some point
private:

	VkRenderPass createRenderPass(RenderPassDesc desc);
	VkDescriptorSetLayout createDescriptorSetLayout(BindingDesc* bindings, size_t count);
	VkPushConstantRange createPushConstantRange(const PushConstantsRange& range);
	void createDescriptorSets(VkDescriptorSetLayout layout, VkDescriptorPool pool, VkDescriptorSet* out_sets, uint32_t count);
	void recordRenderPass(VkCommandBuffer commandBuffer, RenderPass& renderPass);
	void recordComputePass(VkCommandBuffer commandBuffer, ComputePass& renderPass);

public:

	Pipeline createPipeline(PipelineDesc desc);
	Pipeline createComputePipeline(PipelineDesc desc);
	RenderPass createRenderPassAndPipeline(RenderPassDesc renderPassDesc, PipelineDesc pipelineDesc);
	ComputePass createComputePass(ComputePassDesc desc, PipelineDesc pipelineDesc);
	void setRenderPass(RenderPass& renderPass);
	void drawPacket(const MeshPacket& packet);
	void destroyPipeline(const Pipeline& pipeline);
	void destroyRenderPass(const RenderPass& renderPass);
	void destroyComputePass(const ComputePass& computePass);

	void bindVertexBuffer(Buffer& buffer);
	/*Deprecated*/void bindTexture(const GpuImage& image, VkSampler sampler);
	/*Deprecated*/void bindBuffer(const Buffer& buiffer, uint32_t set, uint32_t binding);
	void bindRessources(uint32_t set, std::vector<const Buffer*> buffers, std::vector<ImageBindInfo> images, PipelineType binding_point = PipelineType::Graphics);
	void transitionImage(BarrierDesc desc, PipelineType pipeline_type = PipelineType::Graphics);
	void generateMipmaps(GpuImage& image, PipelineType pipeline_type = PipelineType::Graphics);
	void drawCommand(uint32_t vertex_count);
	void dispatchCommand(uint32_t count_x, uint32_t count_y, uint32_t count_z);
	void pushConstants(const void* data, uint32_t offset, uint32_t size, StageFlags = e_Vertex, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE);

	void recordRenderPass(RenderPass& renderPass);
	void recordComputePass(ComputePass& renderPass);
	void recordImGui();

	VkDescriptorPool createDescriptorPool(BindingDesc* bindingDescs, size_t count);
	void createComputeDescriptorSets(const Pipeline& computePipeline);
};
