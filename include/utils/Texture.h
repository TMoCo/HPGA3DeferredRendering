//
// A texture class for handling texture related operations and data
//

#ifndef TEXTURE_H
#define TEXTURE_H

#include <hpg/VulkanSetup.h>

#include <string> // string class

#include <vulkan/vulkan_core.h>

class Texture {
public:
    void createTexture(VulkanSetup* pVkSetup, const std::string& path, const VkCommandPool& commandPool, const VkFormat& format);

    void cleanupTexture();

private:

    void createTextureImage(const std::string& path, const VkCommandPool& commandPool, const VkFormat& format);

    void createTextureSampler();

public:
    VulkanSetup* vkSetup;

    // a texture
    VkImage textureImage;
    VkImageView textureImageView;

    // some texture data
    int height;
    int width;

    uint32_t mipLevels;

    VkSampler textureSampler; // lets us sample from an image, here the texture
    
    VkDeviceMemory textureImageMemory;
};

#endif // !TEXTURE_H
