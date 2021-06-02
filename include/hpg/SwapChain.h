///////////////////////////////////////////////////////
// SwapChainData class declaration
///////////////////////////////////////////////////////

//
// A class that contains the swap chain setup and data. It has a create function
// to facilitate the recreation when a window is resized. It contains all the variables
// that depend on the VkSwapChainKHR object.
//

#ifndef VULKAN_SWAP_CHAIN_H
#define VULKAN_SWAP_CHAIN_H

#include <common/Model.h>

#include <hpg/VulkanSetup.h> // for referencing the device
#include <hpg/DepthResource.h> // for referencing the depth resource

#include <vector> // vector container

#include <vulkan/vulkan_core.h>

class SwapChain {
public:
    //-Initialisation and cleanup----------------------------------------//    
    void initSwapChain(VulkanSetup* pVkSetup, Model* model, VkDescriptorSetLayout* descriptorSetLayout);
    void cleanupSwapChain();

private:
    //-Swap chain creation helpers---------------------------------------//    
    void createSwapChain();
    VulkanSetup::SwapChainSupportDetails querySwapChainSupport();
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    //-Swap chain image views--------------------------------------------//    
    void createSwapChainImageViews();

    //-Render passes-----------------------------------------------------//    
    void createRenderPass();
    void createImGuiRenderPass();
    
    //-Pipelines---------------------------------------------------------//    
    void createForwardPipeline(VkDescriptorSetLayout* descriptorSetLayout, Model* model);
    void createDeferredPipeline();
    void createCompositionPipeline();

public:
    //-Members-----------------------------------------------------------//    
    VulkanSetup* vkSetup;

    VkSwapchainKHR           swapChain;
    
    VkExtent2D               extent;
    VkFormat                 imageFormat;
    std::vector<VkImage>     images;
    std::vector<VkImageView> imageViews;
    
    VulkanSetup::SwapChainSupportDetails  supportDetails;

    VkRenderPass     renderPass;
    VkRenderPass     imGuiRenderPass;

    VkPipelineLayout pipelineLayout;
    VkPipeline       pipeline;

    bool enableDepthTest = true; // default
};

#endif // !VULKAN_SWAP_CHAIN_H
