///////////////////////////////////////////////////////
// DepthResource class declaration
///////////////////////////////////////////////////////

#ifndef DEPTH_RESOURCE_H
#define DEPTH_RESOURCE_H

#include <hpg/VulkanSetup.h> // for referencing the device
#include <hpg/Image.h>

#include <vulkan/vulkan_core.h>

class DepthResource {
public:
    //-Initialisation and cleanup-----------------------------------------//    
    void createDepthResource(VulkanSetup* vkSetup, const VkExtent2D& extent, VkCommandPool commandPool);
    void cleanupDepthResource();

    //-Depth resource creation helpers------------------------------------//
    static VkFormat findDepthFormat(const VulkanSetup* vkSetup);
    static VkFormat findSupportedFormat(const VulkanSetup* vkSetup, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

public:
    //-Members------------------------------------------------------------//
    VulkanSetup* vkSetup;
	
    VulkanImage depthImage;
    VkImageView depthImageView;
};

#endif // !DEPTH_RESOURCE_H