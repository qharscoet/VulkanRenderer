#include "Device.h"
#include "Pipeline.h"

#include "FileUtils.h"

#include <stdexcept>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <ranges>
#include <algorithm>
#include <numeric>

//#include <glm/gtx/hash.hpp>

inline void hash_combine(size_t& seed, size_t hash)
{
	//TODO : remove the call and steal the algo ?
	//glm::detail::hash_combine(seed, hash);

	hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
	seed ^= hash;
}

static const float debug_colors[][4] = {
	{ 1.0f, 1.0f, 1.0f, 1.0f }, //White
	{ 0.0f, 0.0f, 0.0f, 0.0f }, //Black
	{ 1.0f, 0.0f, 0.0f, 1.0f }, //Red
	{ 0.0f, 1.0f, 0.0f, 1.0f }, //Green
	{ 0.0f, 0.0f, 1.0f, 1.0f }, //Blue
	{ 1.0f, 1.0f, 0.0f, 1.0f }, //Yellow
	{ 1.0f, 0.0f, 1.0f, 1.0f }, //Magenta
	{ 0.0f, 1.0f, 1.0f, 1.0f }, //Cyan
	{ 0.5f, 0.5f, 0.5f, 1.0f }, //Grey
};

static_assert(sizeof(debug_colors) / sizeof(debug_colors[0]) == (int)DebugColor::Nb, "Debug colors array size mismatch");

static const std::string baseShaderPath = "./shaders/spv/";


uint32_t getVkStageFlags(StageFlags stageFlags)
{
	uint32_t vkStageFlags = 0;
	if ( stageFlags & e_Pixel)		vkStageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
	if ( stageFlags & e_Vertex)		vkStageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
	if ( stageFlags & e_Compute)	vkStageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;

	return vkStageFlags;
}

VkDescriptorSetLayout Device::createDescriptorSetLayout(BindingDesc* bindingDescs, size_t count) {
	VkDescriptorSetLayout out_layout;

	std::vector< VkDescriptorSetLayoutBinding> bindings;
	bindings.resize(count);

	for (int i = 0; i < count; i++)
	{
		BindingDesc desc = bindingDescs[i];

		bindings[i].binding = i;
		switch (desc.type)
		{
		case BindingType::UBO: 				bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
		case BindingType::ImageSampler: 	bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; break;
		case BindingType::StorageBuffer: 	bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; break;
		case BindingType::StorageImage:		bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; break;
		default:break;
		}
		bindings[i].descriptorCount = 1;

		uint32_t stageFlags = 0;
		if (desc.stageFlags & e_Pixel)		stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
		if (desc.stageFlags & e_Vertex)		stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
		if (desc.stageFlags & e_Compute)	stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;

		bindings[i].stageFlags = stageFlags;
		bindings[i].pImmutableSamplers = nullptr;
	}
	

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = count;
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &out_layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout");
	}

	return out_layout;
}

VkPushConstantRange Device::createPushConstantRange(const PushConstantsRange& range)
{
	VkPushConstantRange pushConstantRange{};
	uint32_t stageFlags = 0;
	if (range.stageFlags & e_Pixel)		stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
	if (range.stageFlags & e_Vertex)	stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
	if (range.stageFlags & e_Compute)	stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;

	return VkPushConstantRange{
		.stageFlags = stageFlags,
		.offset = range.offset,
		.size = range.size
		};
}

VkDescriptorPool Device::createDescriptorPool(BindingDesc* bindingDescs, size_t count) {

	VkDescriptorPool out_pool;

	std::vector<VkDescriptorPoolSize> poolSizes{};
	poolSizes.resize(count);

	for (int i = 0; i < count; i++)
	{
		BindingDesc desc = bindingDescs[i];

		switch (desc.type)
		{
		case BindingType::UBO: 				poolSizes[i].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
		case BindingType::ImageSampler: 	poolSizes[i].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; break;
		case BindingType::StorageBuffer: 	poolSizes[i].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; break;
		case BindingType::StorageImage: 	poolSizes[i].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; break;
		default:break;
		}

		//TODO do something about the size, at 100 because no reason, to be safe
		poolSizes[i].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 300;
	}

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 300;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &out_pool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create escriptor pool");
	}

	return out_pool;
}

void Device::createDescriptorSets(VkDescriptorSetLayout layout, VkDescriptorPool pool, VkDescriptorSet* out_sets, uint32_t count)
{
	std::vector<VkDescriptorSetLayout> layouts(count, layout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = count;
	allocInfo.pSetLayouts = layouts.data();

	if (vkAllocateDescriptorSets(device, &allocInfo, out_sets) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}
}



//TODO : define properly depth and resolve
VkRenderPass Device::createRenderPass(RenderPassDesc desc)
{
	// This is basically our output for this pass

	VkRenderPass out_renderPass;

	size_t colorAttachment_count = std::max(desc.framebufferDesc.images.size(), (size_t)1);

	std::vector<VkAttachmentDescription> colorAttachments;
	std::vector<VkAttachmentReference> colorAttachmentRefs;
	colorAttachments.resize(colorAttachment_count);
	colorAttachmentRefs.resize(colorAttachment_count);

	VkAttachmentLoadOp loadOp = desc.doClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;

	for (size_t i = 0; i < colorAttachments.size(); i++)
	{
		colorAttachments[i].format = swapChainImageFormat;
		colorAttachments[i].samples = desc.useMsaa ? msaaSamples : VK_SAMPLE_COUNT_1_BIT;
		colorAttachments[i].loadOp = loadOp;
		colorAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachments[i].finalLayout = (desc.useMsaa || !desc.writeSwapChain)? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachments[i].initialLayout = desc.doClear ? VK_IMAGE_LAYOUT_UNDEFINED : colorAttachments[i].finalLayout;

		colorAttachmentRefs[i].attachment = i;
		colorAttachmentRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = findDepthFormat();
	depthAttachment.samples = desc.useMsaa ? msaaSamples : VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = loadOp;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = desc.doClear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = colorAttachment_count;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription colorAttachmentResolve{};
	colorAttachmentResolve.format = swapChainImageFormat;
	colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentResolveRef{};
	colorAttachmentResolveRef.attachment = colorAttachment_count + 1;
	colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = colorAttachment_count;
	subpass.pColorAttachments = colorAttachmentRefs.data();
	subpass.pDepthStencilAttachment = desc.hasDepth? &depthAttachmentRef: nullptr;
	subpass.pResolveAttachments = desc.useMsaa ? &colorAttachmentResolveRef : nullptr;

	std::vector<VkAttachmentDescription> attachments;// = { colorAttachments[0], depthAttachment, colorAttachmentResolve };
	attachments.reserve(colorAttachment_count + desc.hasDepth + desc.useMsaa);
	attachments.insert(attachments.end(), colorAttachments.begin(), colorAttachments.end());
	if (desc.hasDepth)
		attachments.push_back(depthAttachment);
	if (desc.useMsaa)
		attachments.push_back(colorAttachmentResolve);

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;


	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | (desc.hasDepth?VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT:0);
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | (desc.hasDepth?VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT:0);
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | (desc.hasDepth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:0);

	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &out_renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}

	return out_renderPass;
}

Pipeline Device::createPipeline(PipelineDesc desc)
{
	Pipeline out_pipeline;

	auto vertShaderCode = readFile(baseShaderPath + desc.vertexShader);
	auto fragShaderCode = readFile(baseShaderPath + desc.pixelShader);

	const bool isVertHLSL = strstr(desc.vertexShader, ".vs") != NULL || strstr(desc.vertexShader, ".slang") != NULL;
	const bool isFragHLSL = strstr(desc.pixelShader, ".ps") != NULL || strstr(desc.vertexShader, ".slang") != NULL;;

	VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

	SetShaderModuleName(vertShaderModule, desc.vertexShader);
	SetShaderModuleName(fragShaderModule, desc.pixelShader);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = isVertHLSL?"VSMain":"main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = isFragHLSL ? "PSMain":"main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };


	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();


	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	//TODO : get that trhough shader reflection
	vertexInputInfo.vertexBindingDescriptionCount = desc.bindingDescription == nullptr ? 0 : 1;
	vertexInputInfo.pVertexBindingDescriptions = desc.bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = desc.attributeDescriptionsCount;
	vertexInputInfo.pVertexAttributeDescriptions = desc.attributeDescriptions;


	//Triangles only
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	switch (desc.topology)
	{
	case PrimitiveToplogy::PointList: topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
	case PrimitiveToplogy::TriangleList: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
	case PrimitiveToplogy::TriangleStrip: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
	}

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = topology;
	inputAssembly.primitiveRestartEnable = VK_FALSE;


	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;


	//Reminder : activating lots of stuff here like wireframe mode require a GPU feature
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = desc.isWireframe ? VK_POLYGON_MODE_LINE: VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	// MSAA, will see later
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	
	//Depth
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = desc.blendMode == BlendMode::Opaque;
	depthStencil.depthCompareOp = static_cast<VkCompareOp>(desc.depthCompareOp); // VK_COMPARE_OP_LESS
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional

	// Opaque
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments{};
	colorBlendAttachments.resize(desc.attachmentCount);

	for (int i = 0; i < desc.attachmentCount; i++)
	{
		VkPipelineColorBlendAttachmentState& colorBlendAttachment = colorBlendAttachments[i];
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		switch (desc.blendMode)
		{
		case BlendMode::Opaque:
			colorBlendAttachment.blendEnable = VK_FALSE;
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
			colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
			colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
			break;
		case BlendMode::AlphaBlend:
			colorBlendAttachment.blendEnable = VK_TRUE;
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
			break;

		default:
			break;
		}
	}

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = colorBlendAttachments.size();
	colorBlending.pAttachments = colorBlendAttachments.data();
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional


	size_t setCount = desc.bindings.size();

	std::vector<VkDescriptorSetLayout> setLayouts;
	for (int i = 0; i < setCount; i++)
	{
		setLayouts.push_back(createDescriptorSetLayout(desc.bindings[i].data(), desc.bindings[i].size()));
	}

	std::vector<VkPushConstantRange> pushConstantsRanges;

	for (auto& range : desc.pushConstantsRanges)
	{
		pushConstantsRanges.push_back(createPushConstantRange(range));
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = setLayouts.size(); // Optional
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = pushConstantsRanges.size(); // Optional
	pipelineLayoutInfo.pPushConstantRanges = pushConstantsRanges.data(); // Optional

	VkPipelineLayout out_pipelineLayout;
	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &out_pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	//VkRenderPass renderPass = desc.useDefaultRenderPass? defaultRenderPass: createRenderPass(desc.colorAttachment, desc.hasDepth, desc.useMsaa);

	// FINALLY ! 
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;

	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = desc.hasDepth? &depthStencil:nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;

	pipelineInfo.layout = out_pipelineLayout;
	pipelineInfo.renderPass = desc.renderPass;
	pipelineInfo.subpass = 0;

	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &out_pipeline.graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	if (desc.renderPassMsaa != VK_NULL_HANDLE) {
		multisampling.rasterizationSamples = msaaSamples;
		pipelineInfo.renderPass = desc.renderPassMsaa;
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &out_pipeline.graphicsPipelineMsaa) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}
	}

	vkDestroyShaderModule(device, fragShaderModule, nullptr);
	vkDestroyShaderModule(device, vertShaderModule, nullptr);


	out_pipeline.descriptorSetLayouts = setLayouts;
	out_pipeline.renderPass = desc.renderPass;
	out_pipeline.renderPassMsaa = desc.renderPassMsaa;
	out_pipeline.pipelineLayout = out_pipelineLayout;

	if (desc.bindings.size() > 0)
		out_pipeline.descriptorPool = createDescriptorPool(desc.bindings[0].data(), desc.bindings[0].size()); //TODO: reconsider this
	else
		out_pipeline.descriptorPool = createDescriptorPool(nullptr, 0); //TODO: reconsider this

	out_pipeline.descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	out_pipeline.bindings = std::move(desc.bindings);

	return out_pipeline;
}


Pipeline Device::createComputePipeline(PipelineDesc desc)
{
	Pipeline out_pipeline;

	auto computeShaderCode = readFile(baseShaderPath + desc.computeShader);

	VkShaderModule computeShaderModule = createShaderModule(computeShaderCode);

	VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
	computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;

	computeShaderStageInfo.module = computeShaderModule;
	computeShaderStageInfo.pName = "main";


	std::vector<VkDescriptorSetLayout> setLayouts;
	for (auto& bindingSet : desc.bindings)
	{
		setLayouts.push_back(createDescriptorSetLayout(bindingSet.data(), bindingSet.size()));
	}

	std::vector<VkPushConstantRange> pushConstantsRanges;
	for (auto& range : desc.pushConstantsRanges)
	{
		pushConstantsRanges.push_back(createPushConstantRange(range));
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = setLayouts.size(); // Optional
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = pushConstantsRanges.size(); // Optional
	pipelineLayoutInfo.pPushConstantRanges = pushConstantsRanges.data(); // Optional

	VkPipelineLayout computePipelineLayout;
	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

 
	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout = computePipelineLayout;
	pipelineInfo.stage = computeShaderStageInfo;


	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &out_pipeline.graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create compute pipeline!");
	}

	vkDestroyShaderModule(device, computeShaderModule, nullptr);


	out_pipeline.renderPass = VK_NULL_HANDLE;
	out_pipeline.renderPassMsaa = VK_NULL_HANDLE;
	out_pipeline.graphicsPipelineMsaa = VK_NULL_HANDLE;

	out_pipeline.descriptorSetLayouts = setLayouts;
	out_pipeline.pipelineLayout = computePipelineLayout;
	if (desc.bindings.size() > 0)
		out_pipeline.descriptorPool = createDescriptorPool(desc.bindings[0].data(), desc.bindings[0].size()); //TODO: reconsider this
	else
		out_pipeline.descriptorPool = createDescriptorPool(nullptr, 0); //TODO: reconsider this

	out_pipeline.descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	out_pipeline.bindings = std::move(desc.bindings);

	return out_pipeline;
}


RenderPass Device::createRenderPassAndPipeline(RenderPassDesc renderPassDesc, PipelineDesc pipelineDesc)
{
	renderPassDesc.useMsaa = false;
	VkRenderPass renderpass = createRenderPass(renderPassDesc);
	renderPassDesc.useMsaa = true;
	VkRenderPass renderpassMsaa = renderPassDesc.writeSwapChain ? createRenderPass(renderPassDesc) : VK_NULL_HANDLE;

	pipelineDesc.renderPass = renderpass;
	pipelineDesc.renderPassMsaa = renderpassMsaa;
	//pipelineDesc.useMsaa = renderPassDesc.useMsaa;
	pipelineDesc.hasDepth = renderPassDesc.hasDepth;
	pipelineDesc.attachmentCount = std::max(renderPassDesc.framebufferDesc.images.size(), (size_t)1);
	Pipeline pipeline = createPipeline(pipelineDesc);

	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	if (renderPassDesc.framebufferDesc.images.size() > 0)
	{

		auto& images = renderPassDesc.framebufferDesc.images;
		std::vector<VkImageView> imageViews;
		imageViews.reserve(renderPassDesc.framebufferDesc.images.size());
		std::transform(images.begin(), images.end(), std::back_inserter(imageViews), [](const GpuImage* I) {return I->view; });

		if (renderPassDesc.hasDepth && renderPassDesc.framebufferDesc.depth != nullptr)
		{
			imageViews.push_back(renderPassDesc.framebufferDesc.depth->view);
		}

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderpass;
		framebufferInfo.attachmentCount = imageViews.size();
		framebufferInfo.pAttachments = imageViews.data();
		framebufferInfo.width = renderPassDesc.framebufferDesc.width;
		framebufferInfo.height = renderPassDesc.framebufferDesc.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}

	VkDebugUtilsLabelEXT markerInfo = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			.pLabelName = renderPassDesc.debugInfo.name,
	};
	memcpy(markerInfo.color, debug_colors[(int)renderPassDesc.debugInfo.color], sizeof(markerInfo.color));

	SetRenderPassName(renderpass, renderPassDesc.debugInfo.name);

	if (renderpassMsaa != VK_NULL_HANDLE)
		SetRenderPassName(renderpassMsaa, renderPassDesc.debugInfo.name);

	return {
		.renderPass = renderpass,
		.renderPassMsaa = renderpassMsaa,
		.colorAttachement_count = pipelineDesc.attachmentCount,
		.hasDepth = renderPassDesc.hasDepth,
		.pipeline = pipeline,
		.framebuffer = framebuffer,
		.extent = { renderPassDesc.framebufferDesc.width, renderPassDesc.framebufferDesc.height },
		.postDrawBarriers = renderPassDesc.postDrawBarriers,
		.draw = renderPassDesc.drawFunction,
		.markerInfo = markerInfo,
	};
}

ComputePass Device::createComputePass(ComputePassDesc computePassDesc, PipelineDesc pipelineDesc)
{
	Pipeline pipeline = createComputePipeline(pipelineDesc);

	VkDebugUtilsLabelEXT markerInfo = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			.pLabelName = computePassDesc.debugInfo.name,
	};
	memcpy(markerInfo.color, debug_colors[(int)computePassDesc.debugInfo.color], sizeof(markerInfo.color));

	return {
		.pipeline = pipeline,
		.dispatch = computePassDesc.dispatchFunction,
		.markerInfo = markerInfo,
	};
}


void Device::bindVertexBuffer(Buffer& buffer)
{
	VkCommandBuffer commandBuffer = commandBuffers[current_frame];

	VkBuffer vertexBuffers[] = { buffer.buffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
}

void Device::bindTexture(const GpuImage& image, VkSampler sampler)
{
	VkCommandBuffer commandBuffer = commandBuffers[current_frame];

	VkDescriptorPool descriptorPool = currentPipeline->descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout = currentPipeline->descriptorSetLayouts[0];

	std::hash<VkImage> hasher;
	size_t hash = 0;
	hash_combine(hash, hasher(image.image));

	if (currentPipeline->descriptorSetsMap.contains(hash))
	{
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->pipelineLayout, 0, 1, &currentPipeline->descriptorSetsMap[hash], 0, nullptr);
		return;
	}
	else {
		VkDescriptorSet descriptorSet;
		createDescriptorSets(descriptorSetLayout, descriptorPool, &descriptorSet, 1);
		
			//updateDescriptorSet(image.view, sampler, descriptorSet);

			VkDescriptorImageInfo imageInfo{ };
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = image.view;
			imageInfo.sampler = sampler;

			VkWriteDescriptorSet descriptorWrite{};

			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = descriptorSet;
			descriptorWrite.dstBinding = 0;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pBufferInfo = nullptr;
			descriptorWrite.pImageInfo = &imageInfo; // Optional
			descriptorWrite.pTexelBufferView = nullptr; // Optional

			vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
		
		currentPipeline->descriptorSetsMap[hash] = descriptorSet;
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	}

	//std::hash<T> hasher;
	//glm::detail::hash_combine(seed, hasher(v));


	//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->pipelineLayout, 0, 1, &image.descriptorSet, 0, nullptr);
}

void Device::bindBuffer(const Buffer& buffer, uint32_t set, uint32_t binding)
{
	VkCommandBuffer commandBuffer = commandBuffers[current_frame];

	VkDescriptorPool descriptorPool = currentPipeline->descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout = currentPipeline->descriptorSetLayouts[set];

	std::hash<VkBuffer> hasher;
	size_t hash = 0;
	hash_combine(hash, hasher(buffer.buffer));

	if (currentPipeline->descriptorSetsMap.contains(hash))
	{
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->pipelineLayout, set, 1, &currentPipeline->descriptorSetsMap[hash], 0, nullptr);
		return;
	}
	else {
		VkDescriptorSet descriptorSet;
		createDescriptorSets(descriptorSetLayout, descriptorPool, &descriptorSet, 1);

		//updateDescriptorSet(image.view, sampler, descriptorSet);

		VkDescriptorBufferInfo bufferInfo = getDescriptorBufferInfo(buffer);

		VkWriteDescriptorSet descriptorWrite{};

		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;
		descriptorWrite.pImageInfo = nullptr;
		descriptorWrite.pTexelBufferView = nullptr; // Optional

		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

		currentPipeline->descriptorSetsMap[hash] = descriptorSet;
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->pipelineLayout, set, 1, &descriptorSet, 0, nullptr);
	}

}

void Device::bindRessources(uint32_t set, std::vector<const Buffer*> buffers, std::vector<ImageBindInfo> images, PipelineType binding_point)
{
	VkCommandBuffer commandBuffer = binding_point == PipelineType::Graphics ? commandBuffers[current_frame] : computeCommandBuffers[current_frame];

	VkDescriptorPool descriptorPool = currentPipeline->descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout = currentPipeline->descriptorSetLayouts[set];
	std::vector<BindingDesc>& bindings = currentPipeline->bindings[set];

	std::hash<VkImageView> img_hasher;
	std::hash<VkSampler> sampler_hasher;
	std::hash<VkBuffer> buffer_hasher;
	size_t hash = 0;

	auto imageIt = images.begin();
	auto bufferIt = buffers.begin();

	for (const auto& binding : bindings)
	{

		switch (binding.type)
		{
		case BindingType::UBO:
		case BindingType::StorageBuffer:
			hash_combine(hash, buffer_hasher((*bufferIt++)->buffer));
			break;

		case BindingType::ImageSampler:
			hash_combine(hash, sampler_hasher(imageIt->sampl));
			//FALLTHROUGH
		case BindingType::StorageImage:
			hash_combine(hash, img_hasher((imageIt++)->imageview));
			break;
		}
	}


	VkPipelineBindPoint vkBindingPoint = binding_point == PipelineType::Graphics ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE;

	if (currentPipeline->descriptorSetsMap.contains(hash))
	{
		vkCmdBindDescriptorSets(commandBuffer, vkBindingPoint, currentPipeline->pipelineLayout, set, 1, &currentPipeline->descriptorSetsMap[hash], 0, nullptr);
	}
	else {
		VkDescriptorSet descriptorSet;
		createDescriptorSets(descriptorSetLayout, descriptorPool, &descriptorSet, 1);

		std::vector<VkDescriptorImageInfo> descriptorImageInfos;
		std::vector<VkDescriptorBufferInfo> descriptorBufferInfos;
		descriptorImageInfos.reserve(images.size());
		descriptorBufferInfos.reserve(images.size());
		std::transform(images.begin(), images.end(), std::back_inserter(descriptorImageInfos), [&](const ImageBindInfo img) { return getDescriptorImageInfo(img); });
		std::transform(buffers.begin(), buffers.end(), std::back_inserter(descriptorBufferInfos), [&](const Buffer* buff) { return getDescriptorBufferInfo(*buff); });

		//auto descriptorImageInfos = images | std::views::transform([&](const GpuImage* img) { return getDescriptorImageInfo(*img, defaultSampler); });
		//auto descriptorBufferInfos = buffers | std::views::transform([&](const Buffer* buff) { return getDescriptorBufferInfo(*buff); });

		updateDescriptorSet(bindings, descriptorImageInfos, descriptorBufferInfos, descriptorSet);
		currentPipeline->descriptorSetsMap[hash] = descriptorSet;
		vkCmdBindDescriptorSets(commandBuffer, vkBindingPoint, currentPipeline->pipelineLayout, set, 1, &descriptorSet, 0, nullptr);
	}
}

void Device::transitionImage(BarrierDesc desc, PipelineType pipeline_type)
{
	const VkImageLayout layoutMap[ImageLayout::Nb] = {
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	VkCommandBuffer commandBuffer = pipeline_type == PipelineType::Graphics ? commandBuffers[current_frame] : computeCommandBuffers[current_frame];

	// TODO take care of the format
	transitionImageLayout(desc.image->image, VK_FORMAT_R8G8B8A8_SRGB, layoutMap[desc.oldLayout], layoutMap[desc.newLayout], desc.mipLevels, desc.layerCount, commandBuffer);
}

void Device::generateMipmaps(GpuImage& image, PipelineType pipeline_type)
{
	if (pipeline_type == PipelineType::Compute)
	{
		int32_t mipWidth = image.width;
		int32_t mipHeight = image.height;
		uint32_t mipLevels = image.mipLevels;
		uint32_t layerCount = image.layerCount;

		VkCommandBuffer commandBuffer = computeCommandBuffers[current_frame];

		for (uint32_t i = 1; i < mipLevels; i++)
		{

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0,0,0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = layerCount;

			blit.dstOffsets[0] = { 0,0,0 };
			blit.dstOffsets[1] = { std::max(mipWidth / 2, 1), std::max(mipHeight / 2, 1), 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = layerCount;

			vkCmdBlitImage(commandBuffer,
				image.image, VK_IMAGE_LAYOUT_GENERAL,
				image.image, VK_IMAGE_LAYOUT_GENERAL,
				1, &blit,
				VK_FILTER_LINEAR);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}
	} 
	else
	{
		generateMipmaps(image.image, VK_FORMAT_R8G8B8A8_SRGB, image.width, image.height, image.mipLevels, image.layerCount);
	}
}

void Device::drawCommand(uint32_t vertex_count)
{
	VkCommandBuffer commandBuffer = commandBuffers[current_frame];

	vkCmdDraw(commandBuffer, vertex_count, 1, 0, 0);
}

void Device::pushConstants(const void* data, uint32_t offset, uint32_t size, StageFlags stageFlags, VkPipelineLayout pipelineLayout)
{

	VkCommandBuffer commandBuffer = stageFlags & e_Compute ? computeCommandBuffers[current_frame]:commandBuffers[current_frame];

	VkPipelineLayout layout = (pipelineLayout == VK_NULL_HANDLE) ? currentPipeline->pipelineLayout : pipelineLayout;

	vkCmdPushConstants(commandBuffer, layout, getVkStageFlags(stageFlags), offset, size, data);
}

void Device::drawPacket(const MeshPacket& packet)
{

	VkCommandBuffer commandBuffer = commandBuffers[current_frame];

	{
		vkCmdPushConstants(commandBuffer, currentPipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPacket::PushConstantsData), &packet.transform);
	}

	{
		VkBuffer vertexBuffers[] = { packet.vertexBuffer->buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

		if (packet.indexBuffer->buffer != 0)
			vkCmdBindIndexBuffer(commandBuffer, packet.indexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);

	}

	//Actual draw !
	if (packet.indexBuffer->buffer != 0)
		vkCmdDrawIndexed(commandBuffer, packet.indexBuffer->count, 1, 0, 0, 0);
	else
		vkCmdDraw(commandBuffer, packet.vertexBuffer->count, 1, 0, 0);
}


void Device::destroyPipeline(const Pipeline& pipeline)
{
	for (const auto setLayout : pipeline.descriptorSetLayouts)
	{
		vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
	}

	vkDestroyPipeline(device, pipeline.graphicsPipeline, nullptr);

	if(pipeline.graphicsPipelineMsaa)
		vkDestroyPipeline(device, pipeline.graphicsPipelineMsaa, nullptr);

	vkDestroyPipelineLayout(device, pipeline.pipelineLayout, nullptr);
	if(pipeline.descriptorPool)
		vkDestroyDescriptorPool(device, pipeline.descriptorPool, nullptr);
}


void Device::destroyRenderPass(const RenderPass& renderPass)
{
	if (renderPass.renderPass)
	{
		vkDestroyRenderPass(device, renderPass.renderPass, nullptr);
		if (renderPass.renderPassMsaa != VK_NULL_HANDLE)
			vkDestroyRenderPass(device, renderPass.renderPassMsaa, nullptr);

		destroyPipeline(renderPass.pipeline);
	}
}

void Device::destroyComputePass(const ComputePass& computePass)
{
	destroyPipeline(computePass.pipeline);
}

void Device::recordRenderPass(VkCommandBuffer commandBuffer, RenderPass& renderPass)
{
	PushCmdLabel(commandBuffer, &renderPass.markerInfo);
	currentPipeline = &renderPass.pipeline;
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = this->usesMsaa? renderPass.renderPassMsaa:  renderPass.renderPass;
	renderPassInfo.framebuffer = renderPass.framebuffer != VK_NULL_HANDLE ? renderPass.framebuffer : swapChainFramebuffers[current_framebuffer_idx];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = renderPass.framebuffer != VK_NULL_HANDLE ?  renderPass.extent : swapChainExtent;

	std::vector<VkClearValue> clearValues{};
	clearValues.resize(renderPass.colorAttachement_count + (renderPass.hasDepth ? 1 : 0));
	for (int i = 0; i < renderPass.colorAttachement_count; i++)
	{
		clearValues[i].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	}

	if (renderPass.hasDepth)
		clearValues.back().depthStencil = {1.0f, 0};

	renderPassInfo.clearValueCount = clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);


	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->usesMsaa?renderPass.pipeline.graphicsPipelineMsaa:renderPass.pipeline.graphicsPipeline);


	renderPass.draw();

	vkCmdEndRenderPass(commandBuffer);
	EndCmdLabel(commandBuffer);

	for(const auto& barrier : renderPass.postDrawBarriers)
	{
		transitionImage(barrier);
	}

}

void Device::recordRenderPass(RenderPass& renderPass)
{
	if (skipDraw)
		return;

	VkCommandBuffer commandBuffer = commandBuffers[current_frame];
	recordRenderPass(commandBuffer, renderPass);
}

void Device::recordComputePass(VkCommandBuffer commandBuffer, ComputePass& computePass) {

	if (!hasRecorededCompute) {

		vkWaitForFences(device, 1, &computeInFlightFences[current_frame], VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &computeInFlightFences[current_frame]);
		vkResetCommandBuffer(commandBuffer, /*VkCommandBufferResetFlagBits*/ 0);


		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording compute command buffer!");
		}


		hasRecorededCompute = true;
	}
	currentPipeline = &computePass.pipeline;

	PushCmdLabel(commandBuffer, &computePass.markerInfo);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePass.pipeline.graphicsPipeline);
	computePass.dispatch();
	EndCmdLabel(commandBuffer);
}

void Device::recordComputePass(ComputePass& computePass)
{
	VkCommandBuffer commandBuffer = computeCommandBuffers[current_frame];

	recordComputePass(commandBuffer, computePass);
}

void Device::recordImGui()
{
	if (skipDraw)
		return;

	VkCommandBuffer commandBuffer = commandBuffers[current_frame];

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = defaultRenderPass;
	renderPassInfo.framebuffer = swapChainFramebuffers[current_framebuffer_idx];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChainExtent;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

	vkCmdEndRenderPass(commandBuffer);

}