///////////////////////////////////////////////////////
// Texture class declaration
///////////////////////////////////////////////////////

//
// A texture class for handling texture related operations and data.
//

#ifndef TEXTURE_H
#define TEXTURE_H

#include <hpg/Buffers.h>
#include <hpg/Image.h>
#include <hpg/VulkanSetup.h>

#include <string> // string class

#include <vulkan/vulkan_core.h>

class Texture {
public:
    //-Initialisation and cleanup----------------------------------------//    
    void createTexture(VulkanSetup* pVkSetup, const VkCommandPool& commandPool, const Image& imageData);
    void cleanupTexture();

private:
    //-Texture sampler creation------------------------------------------//    
    void createTextureSampler();

public:
    //-Members-----------------------------------------------------------//    
    VulkanSetup* vkSetup;

    VulkanImage    textureImage;
    VkImageView    textureImageView = nullptr;

    VkSampler textureSampler;
};

#endif // !TEXTURE_H
