///////////////////////////////////////////////////////
// FrameBufferData class declaration
///////////////////////////////////////////////////////

//
// A class that contains the data related to a frame buffer.
//

#ifndef FRAMEBUFFER_DATA_H
#define FRAMEBUFFER_DATA_H

#include <hpg/VulkanSetup.h> // for referencing the device
#include <hpg/DepthResource.h> // for referencing the depth resource
#include <hpg/SwapChain.h> // for referencing the swap chain

#include <vulkan/vulkan_core.h>

class FramebufferData {
public:
    //-Initialisation and cleanup-----------------------------------------//    
    void initFramebufferData(VulkanSetup* pVkSetup, const SwapChain* swapChain, const VkCommandPool& commandPool);
    void cleanupFrambufferData();

private:
    //-Framebuffer creation-----------------------------------------------//    
    void createFrameBuffers(const SwapChain* swapChain);
    void createImGuiFramebuffers(const SwapChain* swapChain);

public:
    //-Members------------------------------------------------------------//    
    VulkanSetup* vkSetup;

    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkFramebuffer> imGuiFramebuffers;

    DepthResource depthResource;
};


#endif // !FRAMEBUFFER_DATA_H

