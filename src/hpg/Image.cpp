#include <hpg/Image.h>

void VulkanImage::createImage(const VulkanSetup* vkSetup, const VkCommandPool& commandPool, const VulkanImage::CreateInfo& info) {
    // create image ready to accept data on device
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D; // coordinate system of the texels
    imageInfo.extent.width  = info.width; // the dimensions of the image
    imageInfo.extent.height = info.height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1; // no mip mapping yet
    imageInfo.arrayLayers   = 1; // not in array yet
    imageInfo.format        = info.format; // same format as the pixels is best
    imageInfo.tiling        = info.tiling; // tiling of the pixels, let vulkan lay them out
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = info.usage; // same semantics as during buffer creation, here as destination for the buffer copy
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT; // for multisampling
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE; // used by a single queue family
    imageInfo.flags         = 0; // Optional, for sparse data

    // create the image. The hardware could fail for the format we have specified. We should have a list of acceptable formats and choose the best one depending
    // on the selection of formats supported by the device
    if (vkCreateImage(vkSetup->device, &imageInfo, nullptr, &info.pVulkanImage->image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vkSetup->device, info.pVulkanImage->image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = utils::findMemoryType(&vkSetup->physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(vkSetup->device, &allocInfo, nullptr, &info.pVulkanImage->imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(vkSetup->device, info.pVulkanImage->image, info.pVulkanImage->imageMemory, 0);
}

void VulkanImage::cleanupImage(const VulkanSetup* vkSetup) {
    // destroy the texture image and its memory
    vkDestroyImage(vkSetup->device, image, nullptr);
    vkFreeMemory(vkSetup->device, imageMemory, nullptr);
}

VkImageView VulkanImage::createImageView(const VulkanSetup* vkSetup, const VulkanImage* vulkanImage, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageView imageView;
    
    // then create the image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vulkanImage->image; // different image, should refer to the texture
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // image as 1/2/3D textures and cube maps
    viewInfo.format = format;// how the image data should be interpreted

    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(vkSetup->device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

void VulkanImage::transitionImageLayout(const VulkanSetup* vkSetup, const VulkanImage::LayoutTransitionInfo& transitionData) {

    // images may have different layout that affect how pixels are organised in memory, so we need to specify which layout we are transitioning
    // to and from to lake sure we have the optimal layout for our task
    VkCommandBuffer commandBuffer = utils::beginSingleTimeCommands(&vkSetup->device, transitionData.renderCommandPool);
    // a common way to perform layout transitions is using an image memory barrier, generally for suncing acces to a ressourcem
    // eg make sure write completes before subsequent read, but can transition image layout and transfer queue family ownership

    // the barrier struct, useful for synchronising access resources in the pipeline (no read / write conflicts)
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // specify layout transition
    barrier.oldLayout = transitionData.oldLayout; // can use VK_IMAGE_LAYOUT_UNDEFINED if old layout is of no importance
    barrier.newLayout = transitionData.newLayout;
    // indices of the two queue families
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // explicit if we don't want to as not default value
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    // info about the image
    barrier.image = transitionData.pVulkanImage->image; // the image

    // Determine which aspects of the image are included in the view!
    if (transitionData.newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        // in the case the image is a depth image, then we want the view to contain only the depth aspect
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (utils::hasStencilComponent(transitionData.format)) {
            // also need to include the stencil aspect if avaialble
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else {
        // otherwise we are interested in the colour aspect of the image
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    // not an array and no mipmap 
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // we need to set the access mask and pipeline stages based on the layout in the transition
    // we need to handle two transitions: 
    // undefined -> transfer dest : transfer writes that don't need to wait on anything
    // transfer dest -> shader reading : share read should wait on transfer write

    // declare the stages 
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    // determine which of the two transitions we are executing
    if (transitionData.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && transitionData.newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        // first transition, where the layout is not important, to a layout optimal as destination in a transfer operation
        barrier.srcAccessMask = 0; // don't have to wait on anything for pre barrier operation
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // transfer writes must occur in the pipeline transfer stage, a pseudo-stage where transfers happen
    }
    else if (transitionData.oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && transitionData.newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        // second transition, where the src layout is optimal as the destination of a transfer operation, to a layout optimal for sampling by a shader
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // wait on the transfer to finish writing
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (transitionData.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && transitionData.newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        // third transition, where the src layout is not important and the dst layout is optimal for depth/stencil operations
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    // extend this function for other transitions
    else {
        // unrecognised transition
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        // arrays of pipeline barriers of three available types
        0, nullptr, // memory barriers
        0, nullptr, // buffer memory barriers
        1, &barrier // image memory barriers
    );

    utils::endSingleTimeCommands(&vkSetup->device, &vkSetup->graphicsQueue, &commandBuffer, &transitionData.renderCommandPool);
    
}