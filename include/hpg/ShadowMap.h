///////////////////////////////////////////////////////
// Shadow map class 
///////////////////////////////////////////////////////

#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include <hpg/Image.h>

#include <common/types.h>
#include <common/Model.h>

class ShadowMap {
public:
	struct UBO {
		glm::mat4 depthMVP;
	};
	//-Initialisation and cleanup--------------------------------------------------------------------------------//
	void createShadowMap(VulkanSetup* pVkSetup, VkDescriptorSetLayout* descriptorSetLayout, Model* model, const VkCommandPool& cmdPool);
	void cleanupShadowMap();

	void createAttachment(const VkCommandPool& cmdPool);

	void createShadowMapRenderPass();

	void createShadowMapFrameBuffer();

	void createShadowMapSampler();

	void createShadowMapPipeline(VkDescriptorSetLayout* descriptorSetLayout, Model* model);

	void updateShadowMapUniformBuffer(const UBO& ubo);

public:
	VulkanSetup* vkSetup;

	// an image representing the depth seen from the light's perspective, to be sampled
	VulkanImage vulkanImage;
	VkFormat    format = VK_FORMAT_D16_UNORM;
	VkImageView imageView;
	UI32 extent = 1024;
	F32 depthBiasConstant = 0.005f;
	F32 depthBiasSlope = 0.005f;

	VkSampler depthSampler;

	VkRenderPass shadowMapRenderPass;

	VkFramebuffer shadowMapFrameBuffer;

	VkPipelineLayout layout;
	VkPipeline shadowMapPipeline;

	VulkanBuffer shadowMapUniformBuffer;
};

#endif // !SHADOW_MAP_H