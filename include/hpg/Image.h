///////////////////////////////////////////////////////
// VulkanImage class definition
///////////////////////////////////////////////////////

//
// A wrapper for a VkImage and its associated view and memory, along with some helper 
// image utility static functions
//

#ifndef VULKAN_IMAGE_H
#define VULKAN_IMAGE_H

#include <hpg/Buffers.h>

#include <vulkan/vulkan_core.h>

// POD struct for an image from file (.png, .jpeg, &c)
struct Image {
    uint32_t width;
    uint32_t height;
    VkFormat format;
    Buffer imageData;
};

class VulkanImage {
public:
    //-Texture operation info structs-------------------------------------//
    struct CreateInfo {
        uint32_t              width  = 0;
        uint32_t              height = 0;
        VkFormat              format = VK_FORMAT_UNDEFINED;
        VkImageTiling         tiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageUsageFlags     usage = VK_NULL_HANDLE;
        VkMemoryPropertyFlags properties = VK_NULL_HANDLE;
        VulkanImage*          pVulkanImage = nullptr;
    };

    struct LayoutTransitionInfo {
        VulkanImage* pVulkanImage       = nullptr;
        VkCommandPool renderCommandPool = VK_NULL_HANDLE;
        VkFormat      format            = VK_FORMAT_UNDEFINED;
        VkImageLayout oldLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout newLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    };

public:
    //-Initialisation and cleanup-----------------------------------------//
    static void createImage(const VulkanSetup* vkSetup, const VkCommandPool& commandPool, const CreateInfo& info);
    void cleanupImage(const VulkanSetup* vkSetup);

    //-Image view creation------------------------------------------------//
    static VkImageView createImageView(const VulkanSetup* vkSetup, const VulkanImage* vulkanImage, VkFormat format, VkImageAspectFlags aspectFlags);

    //-Image layout transition--------------------------------------------//
    static void transitionImageLayout(const VulkanSetup* vkSetup, const LayoutTransitionInfo& transitionInfo);

public:
    VkExtent2D     extent      = {0, 0};
    VkFormat       format      = VK_FORMAT_UNDEFINED;
    VkImage        image       = nullptr;
    VkDeviceMemory imageMemory = nullptr;
};

#endif // !VULKAN_IMAGE_H