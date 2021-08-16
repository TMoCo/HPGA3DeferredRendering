//
// Definition of the DepthResource class
//

#include <hpg/DepthResource.h>

// reporting and propagating exceptions
#include <iostream> 
#include <stdexcept>

//////////////////////
//
// Create and destroy the resource
//
//////////////////////

void DepthResource::createDepthResource(VulkanSetup* pVkSetup, const VkExtent2D& extent, VkCommandPool commandPool) {
    vkSetup = pVkSetup;
    // depth image should have the same resolution as the colour attachment, defined by swap chain extent
    VkFormat depthFormat = findDepthFormat(vkSetup); // find a depth format

    // we have the information needed to create an image (the format, usage etc) and an image view
    VulkanImage::ImageCreateInfo info{};
    info.width        = extent.width;
    info.height       = extent.height;
    info.format       = depthFormat;
    info.tiling       = VK_IMAGE_TILING_OPTIMAL;
    info.usage        = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    info.properties   = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    info.pVulkanImage = &depthImage;

    VulkanImage::createImage(vkSetup, commandPool, info);

    VkImageViewCreateInfo imageViewCreateInfo = utils::initImageViewCreateInfo(depthImage.image,
        VK_IMAGE_VIEW_TYPE_2D, depthFormat, {}, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });
    depthImageView = VulkanImage::createImageView(vkSetup, imageViewCreateInfo);

    VulkanImage::LayoutTransitionInfo transitionData{};
    transitionData.pVulkanImage      = &depthImage;
    transitionData.renderCommandPool = commandPool;
    transitionData.format            = depthFormat;
    transitionData.oldLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    transitionData.newLayout         = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // undefined layout is used as the initial layout as there are no existing depth image contents that matter
    VulkanImage::transitionImageLayout(vkSetup, transitionData);
}

void DepthResource::cleanupDepthResource() {
    vkDestroyImageView(vkSetup->device, depthImageView, nullptr);
    depthImage.cleanupImage(vkSetup);
}

//
// Helper functions
//

VkFormat DepthResource::findSupportedFormat(const VulkanSetup* vkSetup, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    // instead of a fixed format, get a list of formats ranked from most to least desirable and iterate through it
    for (VkFormat format : candidates) {
        // query the support of the format by the device
        VkFormatProperties props; // contains three fields
        // linearTilingFeatures: Use cases that are supported with linear tiling
        // optimalTilingFeatures: Use cases that are supported with optimal tiling
        // bufferFeatures : Use cases that are supported for buffer
        vkGetPhysicalDeviceFormatProperties(vkSetup->physicalDevice, format, &props);

        // test if the format is supported
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    // could either return a special value or throw an exception
    throw std::runtime_error("failed to find supported format!");
}

VkFormat DepthResource::findDepthFormat(const VulkanSetup* vkSetup) {
    // return a certain depth format if available
    return findSupportedFormat( 
        vkSetup,
        // list of candidate formats
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        // the desired tiling
        VK_IMAGE_TILING_OPTIMAL,
        // the device format properties flag that we want 
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}


