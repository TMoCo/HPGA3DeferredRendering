//
// FramebufferData class definition
//

#include <hpg/FrameBuffer.h>

// reporting and propagating exceptions
#include <iostream> 
#include <stdexcept>
#include <array> // array container

//////////////////////
//
// Create and destroy the framebuffer data
//
//////////////////////

void FrameBuffer::initFrameBuffer(VulkanSetup* pVkSetup, const SwapChain* swapChainData, const VkCommandPool& commandPool) {
    // update the pointer to the setup data rather than passing as argument to functions
    vkSetup = pVkSetup;
    // first create the depth resource
    depthResource.createDepthResource(vkSetup, swapChainData->extent, commandPool);
    // then create the framebuffers
    createFrameBuffers(swapChainData);
    createImGuiFramebuffers(swapChainData);
}

void FrameBuffer::cleanupFrameBuffers() {
    // cleanup the depth resource
    depthResource.cleanupDepthResource();

    // then desroy the frame buffers
    for (size_t i = 0; i < framebuffers.size(); i++) {
        vkDestroyFramebuffer(vkSetup->device, framebuffers[i], nullptr);
        vkDestroyFramebuffer(vkSetup->device, imGuiFramebuffers[i], nullptr);
    }
}

//////////////////////
//
// The framebuffers
//
//////////////////////

void FrameBuffer::createFrameBuffers(const SwapChain* swapChain) {
    // resize the container to hold all the framebuffers, or image views, in the swap chain
    framebuffers.resize(swapChain->imageViews.size());

    // now loop over the image views and create the framebuffers, also bind the image to the attachment
    for (size_t i = 0; i < swapChain->imageViews.size(); i++) {
        // get the attachment 
        std::array<VkImageView, 2> attachments = {
            swapChain->imageViews[i],
            depthResource.depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = swapChain->renderPass; // which renderpass the framebuffer needs, only one at the moment
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size()); // the number of attachments, or VkImageView objects, to bind to the buffer
        framebufferInfo.pAttachments = attachments.data(); // pointer to the attachment(s)
        framebufferInfo.width = swapChain->extent.width; // specify dimensions of framebuffer depending on swapchain dimensions
        framebufferInfo.height = swapChain->extent.height;
        framebufferInfo.layers = 1; // single images so only one layer

        // attempt to create the framebuffer and place in the framebuffer container
        if (vkCreateFramebuffer(vkSetup->device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void FrameBuffer::createImGuiFramebuffers(const SwapChain* swapChain) {
    // resize the container to hold all the framebuffers, or image views, in the swap chain
    imGuiFramebuffers.resize(swapChain->imageViews.size());

    for (size_t i = 0; i < swapChain->imageViews.size(); i++)
    {
        //VkImageView attachment = swapChainData->imageViews[i];
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = swapChain->imGuiRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &swapChain->imageViews[i]; // only the image view as attachment needed
        framebufferInfo.width = swapChain->extent.width;
        framebufferInfo.height = swapChain->extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vkSetup->device, &framebufferInfo, nullptr, &imGuiFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

