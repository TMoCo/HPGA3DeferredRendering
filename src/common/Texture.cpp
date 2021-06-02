//
// Texture class definition
//

#include <common/Texture.h>

#include <utils/Utils.h> // utils namespace

// image loading
#include <stb_image.h>


void Texture::createTexture(VulkanSetup* pVkSetup, const VkCommandPool& commandPool, const Image& image) {
    vkSetup = pVkSetup;

    VulkanBuffer stagingBuffer; // staging buffer containing image in host memory

    VulkanBuffer::CreateInfo createInfo{};
    createInfo.size = image.imageData.size;
    createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    createInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createInfo.pVulkanBuffer = &stagingBuffer;

    VulkanBuffer::createBuffer(vkSetup, &createInfo);

    void* data;
    vkMapMemory(vkSetup->device, stagingBuffer.memory, 0, image.imageData.size, 0, &data);
    memcpy(data, image.imageData.data, image.imageData.size);
    vkUnmapMemory(vkSetup->device, stagingBuffer.memory);

    // create the image and its memory
    VulkanImage::CreateInfo imgCreateinfo{};
    imgCreateinfo.width        = image.width;
    imgCreateinfo.height       = image.height;
    imgCreateinfo.format       = image.format;
    imgCreateinfo.tiling       = VK_IMAGE_TILING_OPTIMAL;
    imgCreateinfo.usage        = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    imgCreateinfo.properties   = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    imgCreateinfo.pVulkanImage = &textureImage;

    VulkanImage::createImage(vkSetup, commandPool, imgCreateinfo);

    // copy host data to device
    VulkanImage::LayoutTransitionInfo transitionData{};
    transitionData.pVulkanImage = &textureImage;
    transitionData.renderCommandPool = commandPool;
    transitionData.format = image.format;
    transitionData.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitionData.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VulkanImage::transitionImageLayout(vkSetup, transitionData); // specify the initial layout VK_IMAGE_LAYOUT_UNDEFINED

    VulkanBuffer::copyBufferToImage(vkSetup, commandPool, stagingBuffer.buffer, textureImage.image, image.width, image.height);

    // need another transfer to give the shader access to the texture
    transitionData.pVulkanImage = &textureImage;
    transitionData.renderCommandPool = commandPool;
    transitionData.format = image.format;
    transitionData.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transitionData.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VulkanImage::transitionImageLayout(vkSetup, transitionData);

    // cleanup the staging buffer and its memory
    stagingBuffer.cleanupBufferData(vkSetup->device);

    // then create the image view
    textureImageView = VulkanImage::createImageView(vkSetup, &textureImage,  image.format, VK_IMAGE_ASPECT_COLOR_BIT);

    // create the sampler
    createTextureSampler();
}

void Texture::cleanupTexture() {
    // destroy the texture image view and sampler
    vkDestroySampler(vkSetup->device, textureSampler, nullptr);
    vkDestroyImageView(vkSetup->device, textureImageView, nullptr);

    // destroy the texture image and its memory
    textureImage.cleanupImage(vkSetup);
}

void Texture::createTextureSampler() {
    // configure the sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // how to interpolate texels that are magnified or minified
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    // addressing mode
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // VK_SAMPLER_ADDRESS_MODE_REPEAT: Repeat the texture when going beyond the image dimensions.
    // VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: Like repeat, but inverts the coordinates to mirror the image when going beyond the dimensions.
    // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : Take the color of the edge closest to the coordinate beyond the image dimensions.
    // VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE : Like clamp to edge, but instead uses the edge opposite to the closest edge.
    // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER : Return a solid color when sampling beyond the dimensions of the image

    samplerInfo.anisotropyEnable = VK_TRUE; // use unless performance is a concern (IT WILL BE)
    VkPhysicalDeviceProperties properties{}; // can query these here or at beginning for reference
    vkGetPhysicalDeviceProperties(vkSetup->physicalDevice, &properties);
    // limites the amount of texel samples that can be used to calculate final colours, obtain from the device properties
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    // self ecplanatory, can't be an arbitrary colour
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE; // which coordinate system we want to use to address texels! usually always normalised
    // if comparison enabled, texels will be compared to a value and result is used in filtering (useful for shadow maps)
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    // mipmapping fields
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    // now create the configured sampler
    if (vkCreateSampler(vkSetup->device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}