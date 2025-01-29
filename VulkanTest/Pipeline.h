#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>


enum class BlendMode {
	Opaque,
	AlphaBlend
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

enum class PipelineType {
	Graphics,
	Compute
};

struct PipelineDesc {
	
	PipelineType type;
	const char* vertexShader;
	const char* pixelShader;
	const char* computeShader;

	VkVertexInputAttributeDescription* attributeDescriptions;
	uint32_t attributeDescriptionsCount;

	//Descriptors params
	BlendMode blendMode;
	std::vector<BindingDesc> bindings;

	bool isWireframe;

	VkRenderPass renderPass;
	bool useMsaa;
};

//function pointer to a void(void) function
typedef void(*DrawFunctionPtr)();

struct RenderPassDesc
{
	uint8_t colorAttachement_count;
	bool hasDepth;
	bool useMsaa;

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
	
	Pipeline pipeline;

	std::function<void()> draw;
};