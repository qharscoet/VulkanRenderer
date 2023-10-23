#pragma once
#include <vulkan/vulkan.h>


enum class BlendMode {
	Opaque,
	AlphaBlend
};

enum class BindingType {
	UBO,
	ImageSampler
};

enum StageFlags {
	e_Pixel		= (1 << 0),
	e_Vertex	= (1 << 1),
};

struct BindingDesc {
	uint32_t slot;
	BindingType type;
	uint32_t stageFlags;

};
struct PipelineDesc {
	
	const char* vertexShader;
	const char* pixelShader;

	//Descriptors params
	BlendMode blendMode;
	std::vector<BindingDesc> bindings;
	
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