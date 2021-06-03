//
// A convenience header file for constants etc. used in the application
//

#ifndef UTILS_H
#define UTILS_H

#include <stdint.h> // uint32_t

#include <vector> // vector container
#include <string> // string class
#include <optional> // optional wrapper
#include <ios> // streamsize

// reporting and propagating exceptions
#include <iostream> 
#include <stdexcept>

#include <vulkan/vulkan_core.h> // vulkan core structs &c

// vectors, matrices ...
#include <glm/glm.hpp>


/* ********************************************** Debug Copy Paste **********************************************

#ifndef NDEBUG
    std::cout << "DEBUG\t" << __FUNCTION__ << '\n';
    std::cout << v << '\n';
    std::cout << std::endl;
    for (auto& v : arr) {

    }
#endif
    *****************************************************************************************************************/

#ifndef NDEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

//#define VERBOSE
#ifdef VERBOSE
const bool enableVerboseValidation = true;
#else
const bool enableVerboseValidation = false;
#endif

const size_t N_DESCRIPTOR_LAYOUTS = 2;

const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const size_t MAX_FRAMES_IN_FLIGHT = 2;

const uint32_t IMGUI_POOL_NUM = 1000;

namespace utils {

    //-Command queue family info -----------------------------------------//
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily; // queue supporting drawing commands
        std::optional<uint32_t> presentFamily; // queue for presenting image to vk surface 

        inline bool isComplete() {
            // if device supports drawing cmds AND image can be presented to surface
            return graphicsFamily.has_value() && presentFamily.has_value();
        }

        static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    };

    //-Type conversion----------------------------------------------------//
    glm::vec2* toVec2(float* pVec);
    glm::vec3* toVec3(float* pVec);
    glm::vec4* toVec4(float* pVec);

    //-Memory type--------------------------------------------------------//
    uint32_t findMemoryType(const VkPhysicalDevice* physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    //-Begin & end single use cmds----------------------------------------//
    VkCommandBuffer beginSingleTimeCommands(const VkDevice* device, const VkCommandPool& commandPool);
    void endSingleTimeCommands(const VkDevice* device, const VkQueue* queue, const VkCommandBuffer* commandBuffer, const VkCommandPool* commandPool);

    //-Texture operation info structs-------------------------------------//
    bool hasStencilComponent(VkFormat format);

    //-Vulkan struct initialisation---------------------------------------//
    // taken from: https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanInitializers.hpp
    VkPipelineVertexInputStateCreateInfo initPipelineVertexInputStateCreateInfo(
        uint32_t bindingCount,
        VkVertexInputBindingDescription* pVertexBindingDescriptions,
        uint32_t attributesCount, 
        VkVertexInputAttributeDescription* pVertexAttributesDescriptions);

    VkPipelineShaderStageCreateInfo initPipelineShaderStageCreateInfo(
        VkShaderStageFlagBits stage,
        VkShaderModule& shader,
        const char* name);

    VkPipelineInputAssemblyStateCreateInfo initPipelineInputAssemblyStateCreateInfo(
        VkPrimitiveTopology topology,
        VkBool32 restartEnabled,
        VkPipelineInputAssemblyStateCreateFlags flags = 0);

    VkPipelineRasterizationStateCreateInfo initPipelineRasterStateCreateInfo(
        VkPolygonMode polyMode,
        VkCullModeFlags cullMode,
        VkFrontFace frontFace,
        VkPipelineRasterizationStateCreateFlags flags = 0,
        float lineWidth = 1.0f);

    VkPipelineColorBlendStateCreateInfo initPipelineColorBlendStateCreateInfo(
        uint32_t attachmentCount,
        const VkPipelineColorBlendAttachmentState* pAttachments);

    VkPipelineDepthStencilStateCreateInfo initPipelineDepthStencilStateCreateInfo(
        VkBool32 depthTestEnable,
        VkBool32 depthWriteEnable,
        VkCompareOp depthCompareOp);

    VkPipelineViewportStateCreateInfo initPipelineViewportStateCreateInfo(
        uint32_t viewportCount,
        VkViewport* pViewports,
        uint32_t scissorCount,
        VkRect2D* pScissors,
        VkPipelineViewportStateCreateFlags flags = 0);

    VkPipelineMultisampleStateCreateInfo initPipelineMultisampleStateCreateInfo(
        VkSampleCountFlagBits rasterizationSamples,
        VkPipelineMultisampleStateCreateFlags flags = 0);

    VkPipelineLayoutCreateInfo initPipelineLayoutCreateInfo(
        uint32_t layoutCount,
        VkDescriptorSetLayout* layouts,
        VkPipelineLayoutCreateFlags flags = 0);

    VkDescriptorSetLayoutBinding initDescriptorSetLayoutBinding(
        uint32_t binding,
        VkDescriptorType type,
        VkPipelineStageFlags flags = 0);

    VkDescriptorSetAllocateInfo initDescriptorSetAllocInfo(
        VkDescriptorPool pool,
        uint32_t count,
        VkDescriptorSetLayout* pDescSetLayouts);

    VkWriteDescriptorSet initWriteDescriptorSet(
        VkDescriptorSet dst,
        uint32_t binding,
        VkDescriptorType type,
        VkDescriptorBufferInfo* pBufferInfo);

    VkWriteDescriptorSet initWriteDescriptorSet(
        VkDescriptorSet dst,
        uint32_t binding,
        VkDescriptorType type,
        VkDescriptorImageInfo* pImageInfo);
    
}

#endif // !UTILS_H
