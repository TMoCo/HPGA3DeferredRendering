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

// simple light struct
struct Light {
	glm::vec4 pos;
	glm::vec3 color;
	float radius;
};

class GBuffer {
public:
	//-Uniform buffer structs------------------------------------------------------------------------------------//
	struct OffScreenUbo {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 projection;
		glm::mat4 normal;
	};

	struct CompositionUBO {
		glm::vec4 guiData;
		glm::mat4 depthMVP;
		glm::mat4 cameraMVP;
		Light lights[1];
	};

	//-Framebuffer attachment------------------------------------------------------------------------------------//
	struct Attachment {
		VulkanImage vulkanImage{};
		VkFormat    format    = VK_FORMAT_UNDEFINED;
		VkImageView imageView = nullptr;
	};

public:
	//-Initialisation and cleanup--------------------------------------------------------------------------------//
	void createGBuffer(VulkanSetup* pVkSetup, SwapChain* swapChain, VkDescriptorSetLayout* descriptorSetLayout, 
		Model* model, const VkCommandPool& cmdPool);
	void cleanupGBuffer();

	//-Attachment creation---------------------------------------------------------------------------------------//
	void createAttachment(const std::string& name, VkFormat format, VkImageUsageFlagBits usage, const VkCommandPool& cmdPool);
	
	//-Render pass creation--------------------------------------------------------------------------------------//
	void createRenderPass();

	//-Frame buffer creation-------------------------------------------------------------------------------------//
	void createFrameBuffer();

	//-Colour sampler creation-----------------------------------------------------------------------------------//
	void createColourSampler();

	//-Deferred rendering pipeline-------------------------------------------------------------------------------//
	void createPipelines(VkDescriptorSetLayout* descriptorSetLayout, SwapChain* swapChain, Model* model);

	//-Uniform buffer update-------------------------------------------------------------------------------------//
	void updateOffScreenUniformBuffer(const OffScreenUbo& ubo);
	void updateCompositionUniformBuffer(uint32_t imageIndex, const CompositionUBO& ubo);

public:
	//-Members---------------------------------------------------------------------------------------------------//
	VulkanSetup* vkSetup;

	VkExtent2D extent;

	VkRenderPass deferredRenderPass;

	VkFramebuffer deferredFrameBuffer;

	VkSampler colourSampler;

	VulkanBuffer offScreenUniform;
	VulkanBuffer compositionUniforms;

	std::map<std::string, Attachment> attachments;

	VkPipelineLayout layout;
	VkPipeline deferredPipeline;
	VkPipeline offScreenPipeline;
	VkPipeline skyboxPipeline;
};


#endif // !GBUFFER_H
