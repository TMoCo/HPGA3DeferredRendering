#include <hpg/GBuffer.h>
#include <hpg/Shader.h>

#include <utils/Assert.h>

#include <app/AppConstants.h>

void GBuffer::createGBuffer(VulkanSetup* pVkSetup, SwapChain* swapChain, VkDescriptorSetLayout* descriptorSetLayout, 
	Model* model, const VkCommandPool& cmdPool) {
	vkSetup = pVkSetup;
	extent = swapChain->extent; // get extent from swap chain

	createAttachment("position", VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, cmdPool);
	createAttachment("normal", VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, cmdPool);
	createAttachment("albedo", VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, cmdPool);
	createAttachment("depth", DepthResource::findDepthFormat(vkSetup), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, cmdPool);

	createRenderPass();

	createFrameBuffer();

	createColourSampler();

	// uniform buffers
	VulkanBuffer::createUniformBuffer<GBuffer::OffScreenUbo>(vkSetup, 1, 
		&offScreenUniform, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VulkanBuffer::createUniformBuffer<GBuffer::CompositionUBO>(vkSetup, swapChain->images.size(), 
		&compositionUniforms, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	createPipelines(descriptorSetLayout, swapChain, model);
}

void GBuffer::cleanupGBuffer() {
	offScreenUniform.cleanupBufferData(vkSetup->device);
	compositionUniforms.cleanupBufferData(vkSetup->device);

	vkDestroySampler(vkSetup->device, colourSampler, nullptr);
	
	vkDestroyFramebuffer(vkSetup->device, deferredFrameBuffer, nullptr);

	vkDestroyPipeline(vkSetup->device, deferredPipeline, nullptr);
	vkDestroyPipeline(vkSetup->device, offScreenPipeline, nullptr);
	vkDestroyPipeline(vkSetup->device, skyboxPipeline, nullptr);
	vkDestroyPipelineLayout(vkSetup->device, layout, nullptr);

	vkDestroyRenderPass(vkSetup->device, deferredRenderPass, nullptr);

	for (auto& attachment : attachments) {
		vkDestroyImageView(vkSetup->device, attachment.second.imageView, nullptr);
		attachment.second.vulkanImage.cleanupImage(vkSetup);
	}
}

void GBuffer::createAttachment(const std::string& name, VkFormat format, VkImageUsageFlagBits usage, const VkCommandPool& cmdPool) {
	Attachment* attachment = &attachments[name]; // [] inserts an element if non exist in map
	attachment->format = format;

	// create the image 
	VulkanImage::ImageCreateInfo info{};
	info.width        = extent.width;
	info.height       = extent.height;
	info.format       = attachment->format;
	info.tiling       = VK_IMAGE_TILING_OPTIMAL;
	info.usage        = usage | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.pVulkanImage = &attachment->vulkanImage;

	VulkanImage::createImage(vkSetup, cmdPool, info);

	// create the image view
	VkImageAspectFlags aspectMask = 0;
	// usage determines aspect mask
	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) 
		aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (aspectMask <= 0)
		throw std::runtime_error("Invalid aspect mask!");

	VkImageViewCreateInfo imageViewCreateInfo = utils::initImageViewCreateInfo(attachment->vulkanImage.image,
		VK_IMAGE_VIEW_TYPE_2D, format, {}, { aspectMask, 0, 1, 0, 1 });
	attachment->imageView = VulkanImage::createImageView(vkSetup, imageViewCreateInfo);
}

void GBuffer::createRenderPass() {
	// attachment descriptions and references
	std::array<VkAttachmentDescription, 4> attachmentDescriptions = {};

	for (size_t i = 0; i < 4; i++) {
		attachmentDescriptions[i].samples        = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescriptions[i].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescriptions[i].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescriptions[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if (i == 3) {
			attachmentDescriptions[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescriptions[i].finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else {
			attachmentDescriptions[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescriptions[i].finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	attachmentDescriptions[0].format = attachments["position"].format;
	attachmentDescriptions[1].format = attachments["normal"].format;
	attachmentDescriptions[2].format = attachments["albedo"].format;
	attachmentDescriptions[3].format = attachments["depth"].format;

	std::vector<VkAttachmentReference> colourReferences;
	colourReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colourReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colourReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

	VkAttachmentReference depthReference = { 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpass{}; // create subpass
	subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pColorAttachments       = colourReferences.data();
	subpass.colorAttachmentCount    = static_cast<uint32_t>(colourReferences.size());
	subpass.pDepthStencilAttachment = &depthReference;
	
	std::array<VkSubpassDependency, 2> dependencies{}; // dependencies for attachment layout transition

	dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass      = 0;
	dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass      = 0;
	dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pAttachments    = attachmentDescriptions.data();
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
	renderPassInfo.subpassCount    = 1;
	renderPassInfo.pSubpasses      = &subpass;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies   = dependencies.data();

	if (vkCreateRenderPass(vkSetup->device, &renderPassInfo, nullptr, &deferredRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("Could not create GBuffer's render pass");
	}
}

void GBuffer::createFrameBuffer() {
	std::array<VkImageView, 4> attachmentViews;
	attachmentViews[0] = attachments["position"].imageView;
	attachmentViews[1] = attachments["normal"].imageView;
	attachmentViews[2] = attachments["albedo"].imageView;
	attachmentViews[3] = attachments["depth"].imageView;

	VkFramebufferCreateInfo fbufCreateInfo = {};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.pNext           = NULL;
	fbufCreateInfo.renderPass      = deferredRenderPass;
	fbufCreateInfo.pAttachments    = attachmentViews.data();
	fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
	fbufCreateInfo.width           = extent.width;
	fbufCreateInfo.height          = extent.height;
	fbufCreateInfo.layers          = 1;

	if (vkCreateFramebuffer(vkSetup->device, &fbufCreateInfo, nullptr, &deferredFrameBuffer) != VK_SUCCESS) {
		throw std::runtime_error("Could not create GBuffer's frame buffer");
	}
}

void GBuffer::createColourSampler() {
	// Create sampler to sample from the color attachments
	VkSamplerCreateInfo sampler{};
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter     = VK_FILTER_NEAREST;
	sampler.minFilter     = VK_FILTER_NEAREST;
	sampler.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV  = sampler.addressModeU;
	sampler.addressModeW  = sampler.addressModeU;
	sampler.mipLodBias    = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod        = 0.0f;
	sampler.maxLod        = 1.0f;
	sampler.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	if (vkCreateSampler(vkSetup->device, &sampler, nullptr, &colourSampler)) {
		throw std::runtime_error("Could not create GBuffer colour sampler");
	}
}

void GBuffer::createPipelines(VkDescriptorSetLayout* descriptorSetLayout, SwapChain* swapChain, Model* model) {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = utils::initPipelineLayoutCreateInfo(descriptorSetLayout, 1);

	if (vkCreatePipelineLayout(vkSetup->device, &pipelineLayoutCreateInfo, nullptr, &layout) != VK_SUCCESS) {
		throw std::runtime_error("Could not create deferred pipeline layout!");
	}

	VkColorComponentFlags colBlendAttachFlag = 
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	VkPipelineColorBlendAttachmentState colorBlendAttachment = 
		utils::initPipelineColorBlendAttachmentState(colBlendAttachFlag, VK_FALSE);

	VkViewport viewport{ 0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f };
	VkRect2D scissor{ { 0, 0 }, extent };

	VkShaderModule vertShaderModule, fragShaderModule;
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo =
		utils::initPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizerStateInfo =
		utils::initPipelineRasterStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);

	VkPipelineColorBlendStateCreateInfo    colorBlendingStateInfo =
		utils::initPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);

	VkPipelineDepthStencilStateCreateInfo  depthStencilStateInfo =
		utils::initPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo      viewportStateInfo =
		utils::initPipelineViewportStateCreateInfo(1, &viewport, 1, &scissor);

	VkPipelineMultisampleStateCreateInfo   multisamplingStateInfo =
		utils::initPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

	VkPipelineLayoutCreateInfo             pipelineLayoutInfo =
		utils::initPipelineLayoutCreateInfo(1, descriptorSetLayout);
	// shared between the offscreen and composition pipelines

	VkGraphicsPipelineCreateInfo pipelineCreateInfo =
		utils::initGraphicsPipelineCreateInfo(layout, swapChain->renderPass); // composition pipeline uses swapchain render pass

	pipelineCreateInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages             = shaderStages.data();
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateInfo;
	pipelineCreateInfo.pViewportState      = &viewportStateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizerStateInfo;
	pipelineCreateInfo.pMultisampleState   = &multisamplingStateInfo;
	pipelineCreateInfo.pDepthStencilState  = &depthStencilStateInfo;
	pipelineCreateInfo.pColorBlendState    = &colorBlendingStateInfo;

	// composition pipeline
	vertShaderModule = Shader::createShaderModule(vkSetup, Shader::readFile(DEF_VERT_SHADER));
	fragShaderModule = Shader::createShaderModule(vkSetup, Shader::readFile(DEF_FRAG_SHADER));
	shaderStages[0]  = utils::initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule, "main");
	shaderStages[1]  = utils::initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule, "main");

	rasterizerStateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;

	VkPipelineVertexInputStateCreateInfo emptyInputStateInfo = 
		utils::initPipelineVertexInputStateCreateInfo(0, nullptr, 0, nullptr); // no vertex data input
	pipelineCreateInfo.pVertexInputState = &emptyInputStateInfo;

	if (vkCreateGraphicsPipelines(vkSetup->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &deferredPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Could not create deferred graphics pipeline!");
	}

	vkDestroyShaderModule(vkSetup->device, vertShaderModule, nullptr);
	vkDestroyShaderModule(vkSetup->device, fragShaderModule, nullptr);

	// offscreen pipeline
	pipelineCreateInfo.renderPass = deferredRenderPass;

	vertShaderModule = Shader::createShaderModule(vkSetup, Shader::readFile(OFF_VERT_SHADER));
	fragShaderModule = Shader::createShaderModule(vkSetup, Shader::readFile(OFF_FRAG_SHADER));
	shaderStages[0]  = utils::initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule, "main");
	shaderStages[1]  = utils::initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule, "main");

	rasterizerStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;

	auto bindingDescription = model->getBindingDescriptions(0);
	auto attributeDescriptions = model->getAttributeDescriptions(0);

	VkPipelineVertexInputStateCreateInfo   vertexInputStateInfo =
		utils::initPipelineVertexInputStateCreateInfo(1, &bindingDescription,
			static_cast<uint32_t>(attributeDescriptions.size()), attributeDescriptions.data());
	pipelineCreateInfo.pVertexInputState = &vertexInputStateInfo; // vertex input bindings / attributes from gltf model

	std::array<VkPipelineColorBlendAttachmentState, 3> colorBlendAttachmentStates = {
		utils::initPipelineColorBlendAttachmentState(colBlendAttachFlag, VK_FALSE),
		utils::initPipelineColorBlendAttachmentState(colBlendAttachFlag, VK_FALSE),
		utils::initPipelineColorBlendAttachmentState(colBlendAttachFlag, VK_FALSE)
	};

	colorBlendingStateInfo.attachmentCount = static_cast<uint32_t>(colorBlendAttachmentStates.size());
	colorBlendingStateInfo.pAttachments    = colorBlendAttachmentStates.data();
	
	if (vkCreateGraphicsPipelines(vkSetup->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &offScreenPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Could not create deferred graphics pipeline!");
	}

	vkDestroyShaderModule(vkSetup->device, vertShaderModule, nullptr);
	vkDestroyShaderModule(vkSetup->device, fragShaderModule, nullptr);

	// skybox pipeline
	vertShaderModule = Shader::createShaderModule(vkSetup, Shader::readFile(SKY_VERT_SHADER));
	fragShaderModule = Shader::createShaderModule(vkSetup, Shader::readFile(SKY_FRAG_SHADER));
	shaderStages[0] = utils::initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule, "main");
	shaderStages[1] = utils::initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule, "main");

	bindingDescription = { 0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX };
	VkVertexInputAttributeDescription attributeDescription = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };

	vertexInputStateInfo = utils::initPipelineVertexInputStateCreateInfo(1, &bindingDescription, 1, &attributeDescription);
	pipelineCreateInfo.pVertexInputState = &vertexInputStateInfo; // vertex input bindings / attributes from gltf model

	if (vkCreateGraphicsPipelines(vkSetup->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &skyboxPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Could not create deferred graphics pipeline!");
	}

	vkDestroyShaderModule(vkSetup->device, vertShaderModule, nullptr);
	vkDestroyShaderModule(vkSetup->device, fragShaderModule, nullptr);
}

void GBuffer::updateOffScreenUniformBuffer(const GBuffer::OffScreenUbo& ubo) {
	void* data;
	vkMapMemory(vkSetup->device, offScreenUniform.memory, 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(vkSetup->device, offScreenUniform.memory);
}

void GBuffer::updateCompositionUniformBuffer(uint32_t imageIndex, const GBuffer::CompositionUBO& ubo) {
	void* data;
	vkMapMemory(vkSetup->device, compositionUniforms.memory, sizeof(ubo) * imageIndex, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(vkSetup->device, compositionUniforms.memory);
}
