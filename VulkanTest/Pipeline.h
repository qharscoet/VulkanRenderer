#pragma once
#include <vulkan/vulkan.h>


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
	
	//RenderPass params
	bool useDefaultRenderPass;
	uint8_t colorAttachment;
	bool hasDepth;
	bool useMsaa;
};

struct Pipeline {
	VkRenderPass renderPass;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
};