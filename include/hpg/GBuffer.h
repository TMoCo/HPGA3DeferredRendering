///////////////////////////////////////////////////////
// GBuffer class declaration
///////////////////////////////////////////////////////

//
// Following the Sascha Willems example for deferred rendering, this class contains the GBuffer's data.
// https://github.com/SaschaWillems/Vulkan/blob/master/examples/deferred/deferred.cpp
//

#ifndef GBUFFER_H
#define GBUFFER_H

#include <hpg/VulkanSetup.h>
#include <hpg/Buffers.h>
#include <hpg/Image.h>
#include <hpg/SwapChain.h>

#include <vulkan/vulkan_core.h>

#include <map>
#include <string>
#include <type_traits>

class GBuffer {
public:
	//-Uniform buffer struct----------------------------------------------//
	struct UBO {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};

	//-Framebuffer attachment---------------------------------------------//
	struct Attachment {
		VulkanImage image{};
		VkFormat    format    = VK_FORMAT_UNDEFINED;
		VkImageView imageView = nullptr;
	};

public:
	//-Initialisation and cleanup-----------------------------------------//
	void createGBuffer(VulkanSetup* pVkSetup, SwapChain* swapChain, const VkCommandPool& cmdPool);
	void cleanupGBuffer();

	//-Attachment creation------------------------------------------------//
	void createAttachment(const std::string& name, VkFormat format, VkImageUsageFlagBits usage, const VkCommandPool& cmdPool);
	
	//-Render pass reation------------------------------------------------//
	void createDeferredRenderPass();

	//-Frame buffer creation----------------------------------------------//
	void createDeferredFrameBuffer();

	//-Colour sampler creation--------------------------------------------//
	void createColourSampler();

	//-Uniform buffer creation and update---------------------------------//
	void createUniformBuffer();
	void updateUniformBuffer(const UBO& ubo);

public:
	//-Members------------------------------------------------------------//
	VulkanSetup* vkSetup;

	VkExtent2D extent;

	VkRenderPass deferredRenderPass;

	VkFramebuffer deferredFrameBuffer;

	VkSampler colourSampler;

	VulkanBuffer uniformBuffer;

	std::map<std::string, Attachment> attachments;
};


#endif // !GBUFFER_H
