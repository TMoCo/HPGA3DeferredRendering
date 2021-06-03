#include <hpg/GBuffer.h>

#include <utils/Assert.h>

void GBuffer::createGBuffer(VulkanSetup* pVkSetup, SwapChain* swapChain, const VkCommandPool& cmdPool) {
	vkSetup = pVkSetup;
	extent = swapChain->extent; // get extent from swap chain

	createAttachment("position", VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, cmdPool);
	createAttachment("normal", VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, cmdPool);
	createAttachment("albedo", VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, cmdPool);
	createAttachment("depth", DepthResource::findDepthFormat(vkSetup), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, cmdPool);

	createDeferredRenderPass();

	createDeferredFrameBuffer();

	createColourSampler();

	VulkanBuffer::createUniformBuffer<GBuffer::UBO>(vkSetup, 1, &uniformBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void GBuffer::cleanupGBuffer() {
	uniformBuffer.cleanupBufferData(vkSetup->device);

	vkDestroySampler(vkSetup->device, colourSampler, nullptr);

	vkDestroyFramebuffer(vkSetup->device, deferredFrameBuffer, nullptr);

	vkDestroyRenderPass(vkSetup->device, deferredRenderPass, nullptr);

	for (auto& attachment : attachments) {
		vkDestroyImageView(vkSetup->device, attachment.second.imageView, nullptr);
		attachment.second.image.cleanupImage(vkSetup);
	}
}

void GBuffer::createAttachment(const std::string& name, VkFormat format, VkImageUsageFlagBits usage, const VkCommandPool& cmdPool) {
	Attachment* attachment = &attachments[name]; // [] inserts an element if non exist in map
	attachment->format = format;

	// create the image 
	VulkanImage::CreateInfo info{};
	info.width        = extent.width;
	info.height       = extent.height;
	info.format       = attachment->format;
	info.tiling       = VK_IMAGE_TILING_OPTIMAL;
	info.usage        = usage | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.pVulkanImage = &attachment->image;

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

	attachment->imageView = VulkanImage::createImageView(vkSetup, &attachment->image, format, aspectMask);
}

void GBuffer::createDeferredRenderPass() {
	// attachment descriptions and references
	std::vector<VkAttachmentDescription> attachmentDescriptions(attachments.size());
	std::vector<VkAttachmentReference> colourReferences;
	VkAttachmentReference depthReference{};

	uint32_t attachmentNum = 0;
	auto attachment = attachments.begin();
	for (auto& attachmentDescription : attachmentDescriptions) {
		attachmentDescription.samples        = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescription.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescription.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescription.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescription.format         = attachment->second.format;
		
		if (attachment->first == "depth") {
			attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescription.finalLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depthReference = { attachmentNum, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		}
		else {
			attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescription.finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			colourReferences.push_back({ attachmentNum, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		}
		attachmentNum++;
		attachment++; // increment to next attachment
	}

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

void GBuffer::createDeferredFrameBuffer() {
	std::vector<VkImageView> attachmentViews(attachments.size());

	auto attachment = attachments.begin();
	for (auto& attachmentView : attachmentViews) {
		attachmentView = attachment->second.imageView; // get attachment view from attachment
		attachment++;
	}

	VkFramebufferCreateInfo fbufCreateInfo = {};
	fbufCreateInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
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
	sampler.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
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

void GBuffer::updateUniformBuffer(const GBuffer::UBO& ubo) {
	void* data;
	vkMapMemory(vkSetup->device, uniformBuffer.memory, 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(vkSetup->device, uniformBuffer.memory);
}
