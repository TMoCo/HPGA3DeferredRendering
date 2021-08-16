#include<app/AppConstants.h>

#include <hpg/ShadowMap.h>
#include <hpg/Shader.h>

#include <array>

void ShadowMap::createShadowMap(VulkanSetup* pVkSetup, VkDescriptorSetLayout* descriptorSetLayout, Model* model, const VkCommandPool& cmdPool) {
	vkSetup = pVkSetup;
	// create the depth image to sample from
	createAttachment(cmdPool);

	createShadowMapSampler();

	createShadowMapRenderPass();

	createShadowMapFrameBuffer();

	createShadowMapPipeline(descriptorSetLayout, model);

	// uniform buffer
	VulkanBuffer::createUniformBuffer<ShadowMap::UBO>(vkSetup, 1, &shadowMapUniformBuffer,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void ShadowMap::cleanupShadowMap() {

}

void ShadowMap::createAttachment(const VkCommandPool& cmdPool) {
	// create the image 
	VulkanImage::ImageCreateInfo info{};
	info.width = extent;
	info.height = extent;
	info.format = format;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.pVulkanImage = &vulkanImage;

	VulkanImage::createImage(vkSetup, cmdPool, info);

	// create the image view
	VkImageViewCreateInfo imageViewCreateInfo = utils::initImageViewCreateInfo(vulkanImage.image,
		VK_IMAGE_VIEW_TYPE_2D, format, {}, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });

	imageView = VulkanImage::createImageView(vkSetup, imageViewCreateInfo);
}

void ShadowMap::createShadowMapRenderPass() {
	VkAttachmentDescription attachmentDescription{};
	attachmentDescription.format = format;
	attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference depthReference = { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpass{}; // create subpass
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 0;
	subpass.pDepthStencilAttachment = &depthReference;

	std::array<VkSubpassDependency, 2> dependencies{}; // dependencies for attachment layout transition

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 1;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 1;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.pAttachments = &attachmentDescription;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.subpassCount = 1;
	//renderPassCreateInfo.pDependencies = dependencies.data();

	if (vkCreateRenderPass(vkSetup->device, &renderPassCreateInfo, nullptr, &shadowMapRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("Could not create shadow map render pass");
	}
}

void ShadowMap::createShadowMapFrameBuffer() {
	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = shadowMapRenderPass;
	frameBufferCreateInfo.pAttachments = &imageView;
	frameBufferCreateInfo.attachmentCount = 1;
	frameBufferCreateInfo.width = extent;
	frameBufferCreateInfo.height = extent;
	frameBufferCreateInfo.layers = 1;

	if (vkCreateFramebuffer(vkSetup->device, &frameBufferCreateInfo, nullptr, &shadowMapFrameBuffer) != VK_SUCCESS) {
		throw std::runtime_error("Could not create GBuffer's frame buffer");
	}
}

void ShadowMap::createShadowMapSampler() {
	VkFilter filter = VulkanImage::formatIsFilterable(vkSetup->physicalDevice, format, 
		VK_IMAGE_TILING_OPTIMAL) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

	VkSamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	samplerCreateInfo.magFilter = filter;
	samplerCreateInfo.minFilter = filter;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
	samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 1.0f;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	
	if (vkCreateSampler(vkSetup->device, &samplerCreateInfo, nullptr, &depthSampler)) {
		throw std::runtime_error("Could not create GBuffer colour sampler");
	}
}

void ShadowMap::createShadowMapPipeline(VkDescriptorSetLayout* descriptorSetLayout, Model* model) {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = utils::initPipelineLayoutCreateInfo(descriptorSetLayout, 1);

	if (vkCreatePipelineLayout(vkSetup->device, &pipelineLayoutCreateInfo, nullptr, &layout) != VK_SUCCESS) {
		throw std::runtime_error("Could not create shadow map pipeline layout!");
	}

	VkColorComponentFlags colBlendAttachFlag =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
		utils::initPipelineColorBlendAttachmentState(colBlendAttachFlag, VK_FALSE);

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo =
		utils::initPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo =
		utils::initPipelineRasterStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	rasterizationStateCreateInfo.depthBiasEnable = VK_TRUE;

	VkPipelineColorBlendStateCreateInfo    colorBlendStateCreateInfo =
		utils::initPipelineColorBlendStateCreateInfo(0, &colorBlendAttachmentState);

	VkPipelineDepthStencilStateCreateInfo  depthStencilStateCreateInfo =
		utils::initPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo      viewportStateCreateInfo =
		utils::initPipelineViewportStateCreateInfo(1, nullptr, 1, nullptr);

	VkPipelineMultisampleStateCreateInfo   multisampleStateCreateInfo =
		utils::initPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

	auto bindingDescription = model->getBindingDescriptions(0);
	auto attributeDescriptions = model->getAttributeDescriptions(0);

	VkPipelineVertexInputStateCreateInfo   vertexInputStateCreateInfo =
		utils::initPipelineVertexInputStateCreateInfo(1, &bindingDescription,
			static_cast<uint32_t>(attributeDescriptions.size()), attributeDescriptions.data());

	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;

	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = utils::initPipelineDynamicStateCreateInfo(
		dynamicStateEnables.data(), static_cast<UI32>(dynamicStateEnables.size()), 0);

	VkShaderModule vertShaderModule; // , fragShaderModule;
	std::array<VkPipelineShaderStageCreateInfo, 1> shaderStages;
	vertShaderModule = Shader::createShaderModule(vkSetup, Shader::readFile(SHADOWMAP_VERT_SHADER));
	//fragShaderModule = Shader::createShaderModule(vkSetup, Shader::readFile(SHADOWMAP_FRAG_SHADER));
	shaderStages[0] = utils::initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule, "main");
	//shaderStages[1] = utils::initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule, "main");

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = utils::initGraphicsPipelineCreateInfo(layout, shadowMapRenderPass);
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState    = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState   = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pViewportState	   = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState  = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState	   = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.stageCount		   = static_cast<UI32>(shaderStages.size());
	graphicsPipelineCreateInfo.pStages             = shaderStages.data();
	graphicsPipelineCreateInfo.pVertexInputState   = &vertexInputStateCreateInfo; // vertex input bindings / attributes from gltf model
	graphicsPipelineCreateInfo.renderPass = shadowMapRenderPass;

	if (vkCreateGraphicsPipelines(vkSetup->device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &shadowMapPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Could not create deferred graphics pipeline!");
	}

	vkDestroyShaderModule(vkSetup->device, vertShaderModule, nullptr);
	//vkDestroyShaderModule(vkSetup->device, fragShaderModule, nullptr);
}

void ShadowMap::updateShadowMapUniformBuffer(const ShadowMap::UBO& ubo) {
	void* data;
	vkMapMemory(vkSetup->device, shadowMapUniformBuffer.memory, 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(vkSetup->device, shadowMapUniformBuffer.memory);
}