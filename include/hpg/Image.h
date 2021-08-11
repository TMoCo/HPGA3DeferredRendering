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
    struct ImageCreateInfo {
        uint32_t              width  = 0;
        uint32_t              height = 0;
        VkFormat              format = VK_FORMAT_UNDEFINED;
        VkImageTiling         tiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageUsageFlags     usage = VK_NULL_HANDLE;
        uint32_t              arrayLayers = 1; // default to 1 for convenience
        VkMemoryPropertyFlags properties = VK_NULL_HANDLE;
        VkImageCreateFlags    flags = 0;
        VulkanImage*          pVulkanImage = nullptr;
    };

    struct LayoutTransitionInfo {
        VulkanImage*  pVulkanImage      = nullptr;
        VkCommandPool renderCommandPool = VK_NULL_HANDLE;
        VkFormat      format            = VK_FORMAT_UNDEFINED;
        VkImageLayout oldLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout newLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
        uint32_t      arrayLayers       = 1;
    };

    //-Querying a format's support----------------------------------------//
    struct ImageFormatSupportDetails {
        VkFormat format;
        VkImageFormatProperties properties;
    };

public:
    //-Initialisation and cleanup-----------------------------------------//
    static void createImage(const VulkanSetup* vkSetup, const VkCommandPool& commandPool, const ImageCreateInfo& info);
    void cleanupImage(const VulkanSetup* vkSetup);

    //-Image view creation------------------------------------------------//
    static VkImageView createImageView(const VulkanSetup* vkSetup, const VkImageViewCreateInfo& imageViewCreateInfo);

    //-Image layout transition--------------------------------------------//
    static void transitionImageLayout(const VulkanSetup* vkSetup, const LayoutTransitionInfo& transitionInfo);

    //-Image Loading from file--------------------------------------------//
    static Image loadImageFromFile(const std::string& path);

    //-Helpers for image formats------------------------------------------//
    static VkFormat getImageFormat(int numChannels);
    static ImageFormatSupportDetails queryFormatSupport(VkPhysicalDevice device, VkFormat format, VkImageType type, 
        VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags);

public:
    VkExtent2D     extent      = { 0, 0 };
    VkFormat       format      = VK_FORMAT_UNDEFINED;
    VkImage        image       = nullptr;
    VkDeviceMemory imageMemory = nullptr;
};

#endif // !VULKAN_IMAGE_H