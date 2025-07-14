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
	TriangleStrip,
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
	std::vector<std::vector<BindingDesc>> bindings; //vector of sets of bindings

	std::vector<PushConstantsRange> pushConstantsRanges;

	bool isWireframe;

	VkRenderPass renderPass;
	bool hasDepth;
	bool useMsaa;
	uint32_t attachmentCount;
};

struct GpuImage;
struct FramebufferDesc {
	std::vector<const GpuImage*> images;
	GpuImage* depth;
	uint32_t width;
	uint32_t height;
};

enum ImageLayout {
	Undefined,
	TransferSrc,
	TransferDst,
	RenderTarget,
	ShaderRead,
	SwapChain,

	Nb
};

struct BarrierDesc {
	const GpuImage* image;
	ImageLayout oldLayout;
	ImageLayout newLayout;
	uint32_t mipLevels;
};


enum class DebugColor {
	White,
	Black,
	Red,
	Green,
	Blue,
	Yellow,
	Magenta,
	Cyan,
	Grey,

	Nb,
};

struct DebugMarkerInfo {
	const char* name;
	enum DebugColor color;
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
	bool writeSwapChain;

	std::function<void()> drawFunction;
	std::vector<BarrierDesc> postDrawBarriers;
	DebugMarkerInfo debugInfo;
};

struct Pipeline {
	VkRenderPass renderPass;
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	std::vector<VkDescriptorSet> descriptorSets;
	std::unordered_map<size_t, VkDescriptorSet> descriptorSetsMap;
	std::vector<std::vector<BindingDesc>> bindings;
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

	std::vector<BarrierDesc> postDrawBarriers;

	std::function<void()> draw;

	VkDebugUtilsLabelEXT markerInfo;
};