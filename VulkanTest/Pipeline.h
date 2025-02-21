#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>

enum class BlendMode {
	Opaque,
	AlphaBlend
};

enum class PrimitiveToplogy {
	PointList,
	TriangleList,
};

enum class BindingType {
	UBO,
	ImageSampler,
	StorageBuffer
};

enum StageFlags {
	e_Pixel		= (1 << 0),
	e_Vertex	= (1 << 1),
	e_Compute	= (1 << 2),
};

struct BindingDesc {
	uint32_t slot;
	BindingType type;
	uint32_t stageFlags;
};

struct PushConstantsRange {
	uint32_t offset;
	uint32_t size;
	StageFlags stageFlags;
};

enum class PipelineType {
	Graphics,
	Compute
};

struct PipelineDesc {
	
	PipelineType type;
	const char* vertexShader;
	const char* pixelShader;
	const char* computeShader;

	VkVertexInputBindingDescription* bindingDescription;
	VkVertexInputAttributeDescription* attributeDescriptions;
	uint32_t attributeDescriptionsCount;

	//Descriptors params
	BlendMode blendMode;
	PrimitiveToplogy topology;
	std::vector<BindingDesc> bindings;

	std::vector<PushConstantsRange> pushConstantsRanges;

	bool isWireframe;

	VkRenderPass renderPass;
	bool hasDepth;
	bool useMsaa;
	uint32_t attachmentCount;
};

struct GpuImage;
struct FramebufferDesc {
	std::vector<GpuImage> images;
	GpuImage* depth;
	uint32_t width;
	uint32_t height;
};

//function pointer to a void(void) function
typedef void(*DrawFunctionPtr)();

struct RenderPassDesc
{
	FramebufferDesc framebufferDesc;
	uint8_t colorAttachement_count;
	bool hasDepth;
	bool useMsaa;
	bool doClear;

	std::function<void()> drawFunction;
};

struct Pipeline {
	VkRenderPass renderPass;
	VkDescriptorSetLayout descriptorSetLayout;
	std::vector<VkDescriptorSet> descriptorSets;
	VkDescriptorPool descriptorPool;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
};

struct RenderPass {
	VkRenderPass renderPass;
	uint32_t colorAttachement_count;
	bool hasDepth;
	
	Pipeline pipeline;

	//TODO : put that somewhere else
	VkFramebuffer framebuffer;
	VkExtent2D extent;

	std::function<void()> draw;
};