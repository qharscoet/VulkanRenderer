#include "Device.h"
#include "Pipeline.h"

#include "FileUtils.h"

#include <stdexcept>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>


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
		default:break;
		}

		//TODO do something about the size, at 100 because no reason, to be safe
		poolSizes[i].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 100;
	}

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 100;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &out_pool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create escriptor pool");
	}

	return out_pool;
}

void Device::createDescriptorSets(VkDescriptorSetLayout layout, VkDescriptorPool pool, VkDescriptorSet* out_sets)
{
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, layout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
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

	std::vector<VkAttachmentDescription> colorAttachments;
	std::vector<VkAttachmentReference> colorAttachmentRefs;
	colorAttachments.resize(desc.colorAttachement_count);
	colorAttachmentRefs.resize(desc.colorAttachement_count);

	VkAttachmentLoadOp loadOp = desc.doClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;

	for (size_t i = 0; i < colorAttachments.size(); i++)
	{
		colorAttachments[i].format = swapChainImageFormat;
		colorAttachments[i].samples = desc.useMsaa ?msaaSamples: VK_SAMPLE_COUNT_1_BIT;
		colorAttachments[i].loadOp = loadOp;
		colorAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachments[i].finalLayout = desc.useMsaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachments[i].initialLayout = desc.doClear? VK_IMAGE_LAYOUT_UNDEFINED: colorAttachments[i].finalLayout;

		colorAttachmentRefs[i].attachment = i;
		colorAttachmentRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = findDepthFormat();
	depthAttachment.samples = desc.useMsaa ? msaaSamples : VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = loadOp;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = desc.doClear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = desc.colorAttachement_count;
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
	colorAttachmentResolveRef.attachment = desc.colorAttachement_count + 1;
	colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = desc.colorAttachement_count;
	subpass.pColorAttachments = colorAttachmentRefs.data();
	subpass.pDepthStencilAttachment = desc.hasDepth? &depthAttachmentRef: nullptr;
	subpass.pResolveAttachments = desc.useMsaa ? &colorAttachmentResolveRef : nullptr;

	std::array<VkAttachmentDescription, 3> attachments = { colorAttachments[0], depthAttachment, colorAttachmentResolve };
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1 + desc.hasDepth + desc.useMsaa;// attachments.size();
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

	auto vertShaderCode = readFile(desc.vertexShader);
	auto fragShaderCode = readFile(desc.pixelShader);

	VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

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
	auto bindingDescription = Vertex::getBindingDescription();

	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = desc.attributeDescriptionsCount;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = desc.attributeDescriptions;


	//Triangles only
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
	multisampling.rasterizationSamples = desc.useMsaa ? msaaSamples : VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	//Depth
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional

	// Opaque
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
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

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	VkDescriptorSetLayout setLayout = createDescriptorSetLayout(desc.bindings.data(), desc.bindings.size());

	std::vector<VkPushConstantRange> pushConstantsRanges;

	for (auto& range : desc.pushConstantsRanges)
	{
		VkPushConstantRange pushConstantRange{};
		uint32_t stageFlags = 0;
		if (range.stageFlags & e_Pixel)		stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
		if (range.stageFlags & e_Vertex)	stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
		if (range.stageFlags & e_Compute)	stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;

		pushConstantsRanges.push_back(VkPushConstantRange{
			.stageFlags = stageFlags,
			.offset = range.offset,
			.size = range.size
			});
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1; // Optional
	pipelineLayoutInfo.pSetLayouts = &setLayout;
	pipelineLayoutInfo.pushConstantRangeCount = pushConstantsRanges.size(); // Optional
	pipelineLayoutInfo.pPushConstantRanges = pushConstantsRanges.data(); // Optional

	VkPipelineLayout out_pipelineLayout;
	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &out_pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	//VkRenderPass renderPass = desc.useDefaultRenderPass? defaultRenderPass: createRenderPass(desc.colorAttachment, desc.hasDepth, desc.useMsaa);
	VkRenderPass renderPass = desc.renderPass;

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
	pipelineInfo.pDepthStencilState = &depthStencil; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;

	pipelineInfo.layout = out_pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &out_pipeline.graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(device, fragShaderModule, nullptr);
	vkDestroyShaderModule(device, vertShaderModule, nullptr);


	out_pipeline.descriptorSetLayout = setLayout;
	out_pipeline.renderPass = renderPass;
	out_pipeline.pipelineLayout = out_pipelineLayout;
	out_pipeline.descriptorPool = createDescriptorPool(desc.bindings.data(), desc.bindings.size());;

	out_pipeline.descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	createDescriptorSets(setLayout, out_pipeline.descriptorPool, out_pipeline.descriptorSets.data());

	return out_pipeline;
}


Pipeline Device::createComputePipeline(PipelineDesc desc)
{
	Pipeline out_pipeline;

	auto computeShaderCode = readFile(desc.computeShader);

	VkShaderModule computeShaderModule = createShaderModule(computeShaderCode);

	VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
	computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;

	computeShaderStageInfo.module = computeShaderModule;
	computeShaderStageInfo.pName = "main";



	VkDescriptorSetLayout setLayout = createDescriptorSetLayout(desc.bindings.data(), desc.bindings.size());
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1; // Optional
	pipelineLayoutInfo.pSetLayouts = &setLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

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


	out_pipeline.descriptorSetLayout = setLayout;
	out_pipeline.renderPass = VK_NULL_HANDLE;
	out_pipeline.pipelineLayout = computePipelineLayout;
	out_pipeline.descriptorPool = VK_NULL_HANDLE;

	return out_pipeline;
}


RenderPass Device::createRenderPassAndPipeline(RenderPassDesc renderPassDesc, PipelineDesc pipelineDesc)
{
	VkRenderPass renderpass = createRenderPass(renderPassDesc);

	pipelineDesc.renderPass = renderpass;
	pipelineDesc.useMsaa = renderPassDesc.useMsaa;
	Pipeline pipeline = createPipeline(pipelineDesc);

	return { renderpass, pipeline, renderPassDesc.drawFunction };
}

void Device::setRenderPass(RenderPass renderPass)
{
	currentRenderPass = renderPass;
}

void Device::setNextRenderPass(RenderPass renderPass)
{
	nextRenderPass = renderPass;
}

MeshPacket Device::createPacket(Mesh& mesh, Texture& tex)
{
	MeshPacket out_packet;
	out_packet.vertexBuffer = this->createVertexBuffer(mesh.vertices.size() * sizeof(mesh.vertices[0]), (void*)mesh.vertices.data());
	out_packet.indexBuffer = this->createIndexBuffer(mesh.indices.size() * sizeof(mesh.indices[0]), (void*)mesh.indices.data());


	out_packet.texture = this->createTexture(tex);
	out_packet.sampler = this->createTextureSampler(out_packet.texture.mipLevels);

	createDescriptorSets(currentRenderPass.pipeline.descriptorSetLayout, currentRenderPass.pipeline.descriptorPool, &out_packet.internalData.descriptorSet);
	updateDescriptorSet(out_packet.texture.view, out_packet.sampler, out_packet.internalData.descriptorSet);

	out_packet.data.model = glm::mat4(1.0f);

	return out_packet;
}

void Device::drawPacket(MeshPacket packet)
{
	//updateDescriptorSet(packet.texture.view, packet.sampler, currentRenderPass.pipeline.descriptorSets[current_frame]);

	VkCommandBuffer commandBuffer = commandBuffers[current_frame];


	//UBO
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentRenderPass.pipeline.pipelineLayout, 0, 1, &packet.internalData.descriptorSet, 0, nullptr);

	vkCmdPushConstants(commandBuffer, currentRenderPass.pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPacket::PushConstantsData), &packet.data);

	VkBuffer vertexBuffers[] = { packet.vertexBuffer.buffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

	if (packet.indexBuffer.buffer != 0)
		vkCmdBindIndexBuffer(commandBuffer, packet.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	//Actual draw !
	if (packet.indexBuffer.buffer != 0)
		vkCmdDrawIndexed(commandBuffer, packet.indexBuffer.count, 1, 0, 0, 0);
	else
		vkCmdDraw(commandBuffer, PARTICLE_COUNT, 1, 0, 0);
}

void Device::destroyPipeline(Pipeline pipeline)
{
	vkDestroyDescriptorSetLayout(device, pipeline.descriptorSetLayout, nullptr);
	vkDestroyPipeline(device, pipeline.graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipeline.pipelineLayout, nullptr);
	if(pipeline.descriptorPool)
		vkDestroyDescriptorPool(device, pipeline.descriptorPool, nullptr);
}


void Device::destroyRenderPass(RenderPass renderPass)
{
	vkDestroyRenderPass(device, renderPass.renderPass, nullptr);
	destroyPipeline(renderPass.pipeline);
}


void Device::recordRenderPass(VkCommandBuffer commandBuffer)
{

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = currentRenderPass.renderPass;
	renderPassInfo.framebuffer = swapChainFramebuffers[current_framebuffer_idx];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChainExtent;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentRenderPass.pipeline.graphicsPipeline);

	VkBuffer vertexBuffers[] = { vertexBuffer.buffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

	if (indexBuffer.buffer != 0)
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	//UBO
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentRenderPass.pipeline.pipelineLayout, 0, 1, &currentRenderPass.pipeline.descriptorSets[current_frame], 0, nullptr);

	//Actual draw !
	if (indexBuffer.buffer != 0)
		vkCmdDrawIndexed(commandBuffer, index_count, 1, 0, 0, 0);
	else
		vkCmdDraw(commandBuffer, PARTICLE_COUNT, 1, 0, 0);

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

	vkCmdEndRenderPass(commandBuffer);

}

void Device::recordRenderPass(VkCommandBuffer commandBuffer, RenderPass renderPass)
{

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass.renderPass;
	renderPassInfo.framebuffer = swapChainFramebuffers[current_framebuffer_idx];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChainExtent;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);


	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass.pipeline.graphicsPipeline);


	renderPass.draw();

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

	vkCmdEndRenderPass(commandBuffer);

}

void Device::recordImGui(VkCommandBuffer commandBuffer)
{

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


	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

	vkCmdEndRenderPass(commandBuffer);

}