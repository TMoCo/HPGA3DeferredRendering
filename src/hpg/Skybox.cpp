///////////////////////////////////////////////////////
//
// Skybox class definition
//
///////////////////////////////////////////////////////

#include <app/AppConstants.h>

#include <utils/Print.h>

#include <hpg/Skybox.h>
#include <hpg/Buffers.h>

#include <stb_image.h>

void Skybox::createSkybox(VulkanSetup* pVkSetup, const VkCommandPool& commandPool) {
    vkSetup = pVkSetup;

    createSkyboxImage(commandPool);

    createSkyboxImageView();

    createSkyboxSampler();

    VulkanBuffer::createDeviceLocalBuffer(vkSetup, commandPool,
        Buffer{ (unsigned char*)Skybox::cubeVerts, 36 * sizeof(glm::vec3) }, // vertex data as buffer of bytes
        &vertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    VulkanBuffer::createUniformBuffer<Skybox::UBO>(vkSetup, 1, &uniformBuffer,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void Skybox::cleanupSkybox() {
    uniformBuffer.cleanupBufferData(vkSetup->device);
    vertexBuffer.cleanupBufferData(vkSetup->device);

    vkDestroySampler(vkSetup->device, skyboxSampler, nullptr);
    vkDestroyImageView(vkSetup->device, skyboxImageView, nullptr);

    // destroy the skyimage and its memory
    skyboxImage.cleanupImage(vkSetup);
}

void Skybox::createSkyboxImage(const VkCommandPool& commandPool) {
    // load the skybox data from the 6 images
    Image image{};
    const char* faces[6] = { "Right.png", "Left.png", "Bottom.png", "Top.png", "Front.png", "Back.png" };

    // query the dimensions of the file
    int width, height, channels;
    if (!stbi_info((SKYBOX_PATH + faces[0]).c_str(), &width, &height, &channels)) {
        throw std::runtime_error("Could not load desired image file!");
    }

    // use these to assign the width and height of the whole image, and allocate an array of bytes accordingly
    image.height = height;
    image.width = width;
    image.format = VulkanImage::getImageFormat(channels);
    image.imageData.size = height * width * channels * 6; // 6 images of dimensions w x h with pixels of n channels
    image.imageData.data = (unsigned char*)malloc(image.imageData.size);

    PRINT("size of array: %zi\n size of uc: %zi\n", image.imageData.size * sizeof(unsigned char), sizeof(unsigned char));

    if (!image.imageData.data) {
        throw std::runtime_error("Error, could not allocate memory!");
    }

    stbi_set_flip_vertically_on_load(true);

    // for each face, load the pixels and copy them into the image data
    uint32_t offset = 0;
    for (int i = 0; i < 6; i++) {

        unsigned char* data = stbi_load((SKYBOX_PATH + faces[i]).c_str(), &width, &height, &channels, 0);
        if (!data) {
            throw std::runtime_error("Could not load desired image file!");
        }        
        memcpy(image.imageData.data + offset, data, height * width * channels);
        stbi_image_free(data);
        offset += height * width * channels;
    }

    VulkanBuffer stagingBuffer{};

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
    VulkanImage::ImageCreateInfo imgCreateInfo{};
    imgCreateInfo.width = image.width;
    imgCreateInfo.height = image.height;
    imgCreateInfo.format = image.format;
    imgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgCreateInfo.usage = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    imgCreateInfo.arrayLayers = 6;
    imgCreateInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    imgCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    imgCreateInfo.pVulkanImage = &skyboxImage;

    // DEBUG looking at supported formats
    std::vector<VkFormat> formats = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8_SRGB };

    for (auto& format : formats) {
        auto details = VulkanImage::queryFormatSupport(vkSetup->physicalDevice, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
        PRINT("format %i details:\nmax array layers = %i\n", format, details.properties.maxArrayLayers);
    }

    VulkanImage::createImage(vkSetup, commandPool, imgCreateInfo);

    // copy host data to device
    VulkanImage::LayoutTransitionInfo transitionData{};
    transitionData.pVulkanImage = &skyboxImage;
    transitionData.renderCommandPool = commandPool;
    transitionData.format = image.format;
    transitionData.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transitionData.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transitionData.arrayLayers = 6;

    VulkanImage::transitionImageLayout(vkSetup, transitionData); // specify the initial layout VK_IMAGE_LAYOUT_UNDEFINED

    std::vector<VkBufferImageCopy> regions;

    offset = 0;
    // loop over the 6 faces and create the buffer copy regions
    for (int i = 0; i < 6; i++) {
        VkBufferImageCopy region = {};
        region.bufferOffset = offset;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.baseArrayLayer = i;
        region.imageSubresource.layerCount = 1;
        region.imageSubresource.mipLevel = 0;
        region.imageExtent = { image.width, image.height, 1 };
        regions.push_back(region);
        // increment offset into staging buffer
        offset += image.width * image.height * channels;
    }

    VulkanBuffer::copyBufferToImage(vkSetup, commandPool, stagingBuffer.buffer, skyboxImage.image, regions);

    // need another transfer to give the shader access to the texture
    transitionData.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transitionData.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VulkanImage::transitionImageLayout(vkSetup, transitionData);

    // cleanup the staging buffer and its memory
    stagingBuffer.cleanupBufferData(vkSetup->device);
    // cleanup image pixels still in host memory
    free(image.imageData.data);
}

void Skybox::createSkyboxImageView() {
    // image view for the cube image
    VkImageViewCreateInfo imageViewCreateInfo = utils::initImageViewCreateInfo(skyboxImage.image,
        VK_IMAGE_VIEW_TYPE_CUBE, skyboxImage.format,
        VkComponentMapping{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 });
    skyboxImageView = VulkanImage::createImageView(vkSetup, imageViewCreateInfo);
}

void Skybox::createSkyboxSampler() {
    // image sampler
    VkSamplerCreateInfo samplerCreateInfo =
        utils::initSamplerCreateInfo(vkSetup->deviceProperties.limits.maxSamplerAnisotropy);
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
    samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeV;
    if (vkCreateSampler(vkSetup->device, &samplerCreateInfo, nullptr, &skyboxSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}
