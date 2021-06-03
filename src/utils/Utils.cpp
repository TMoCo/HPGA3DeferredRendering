//
// the utils namespace definition
//

#include <utils/Utils.h>

#include <glm/gtc/type_ptr.hpp> // construct vec from ptr

namespace utils {
    QueueFamilyIndices QueueFamilyIndices::findQueueFamilies(const VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
        QueueFamilyIndices indices;

        // similar to physical device and extensions and layers....
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        // create a vector to store queue families
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // if the queue supports the desired queue operation, then the bitwise & operator returns true
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                // gaphics family was assigned a value! optional wrapper has_value now returns true.
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            // checks device queuefamily can present on the surface
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }

            // return the first valid queue family
            if (indices.isComplete()) {
                break;
            }
            i++;
        }
        return indices;
    }


    glm::vec2* toVec2(float* pVec) {
        return reinterpret_cast<glm::vec2*>(pVec);
    }

    glm::vec3* toVec3(float* pVec) {
        return reinterpret_cast<glm::vec3*>(pVec);
    }

    glm::vec4* toVec4(float* pVec) {
        return reinterpret_cast<glm::vec4*>(pVec);
    }

    uint32_t findMemoryType(const VkPhysicalDevice* physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        // find best type of memory
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(*physicalDevice, &memProperties);
        // two arrays in the struct, memoryTypes and memoryHeaps. Heaps are distinct ressources like VRAM and swap space in RAM
        // types exist within these heaps

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            // we want a memory type that is suitable for the vertex buffer, but also able to write our vertex data to memory
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        // otherwise we can't find the right type!
        throw std::runtime_error("failed to find suitable memory type!");
    }

    VkCommandBuffer beginSingleTimeCommands(const VkDevice* device, const VkCommandPool& commandPool) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool        = commandPool;
        allocInfo.commandBufferCount = 1;

        // allocate the command buffer
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(*device, &allocInfo, &commandBuffer);

        // set the struct for the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // command buffer usage 

        // start recording the command buffer
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }

    void endSingleTimeCommands(const VkDevice* device, const VkQueue* queue, const VkCommandBuffer* commandBuffer, const VkCommandPool* commandPool) {
        // end recording
        vkEndCommandBuffer(*commandBuffer);

        // execute the command buffer by completing the submitinfo struct
        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = commandBuffer;

        // submit the queue for execution
        vkQueueSubmit(*queue, 1, &submitInfo, VK_NULL_HANDLE);
        // here we could use a fence to schedule multiple transfers simultaneously and wait for them to complete instead
        // of executing all at the same time, alternatively use wait for the queue to execute
        vkQueueWaitIdle(*queue);

        // free the command buffer once the queue is no longer in use
        vkFreeCommandBuffers(*device, *commandPool, 1, commandBuffer);
    }

    bool hasStencilComponent(VkFormat format) {
        // depth formats with stencil components
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT; 
    }

    VkPipelineInputAssemblyStateCreateInfo initPipelineInputAssemblyStateCreateInfo(
        VkPrimitiveTopology topology, 
        VkBool32 restartEnabled,
        VkPipelineInputAssemblyStateCreateFlags flags) {
        VkPipelineInputAssemblyStateCreateInfo info{};
        info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        info.topology               = topology;
        info.flags                  = flags;
        info.primitiveRestartEnable = restartEnabled;
        return info;
    }

    VkPipelineRasterizationStateCreateInfo initPipelineRasterStateCreateInfo(
        VkPolygonMode polyMode, 
        VkCullModeFlags cullMode, 
        VkFrontFace frontFace,
        VkPipelineRasterizationStateCreateFlags flags,
        float lineWidth) {
        VkPipelineRasterizationStateCreateInfo info{};
        info.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        info.polygonMode = polyMode;
        info.cullMode    = cullMode;
        info.frontFace   = frontFace;
        info.lineWidth   = lineWidth;
        info.flags       = flags;
        return info;
    }


    VkPipelineColorBlendStateCreateInfo initPipelineColorBlendStateCreateInfo(
        uint32_t attachmentCount,
        const VkPipelineColorBlendAttachmentState* pAttachments) {
        VkPipelineColorBlendStateCreateInfo info{};
        info.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        info.attachmentCount = attachmentCount;
        info.pAttachments    = pAttachments;
        return info;
    }

    VkPipelineDepthStencilStateCreateInfo initPipelineDepthStencilStateCreateInfo(
        VkBool32 depthTestEnable,
        VkBool32 depthWriteEnable,
        VkCompareOp depthCompareOp) {
        VkPipelineDepthStencilStateCreateInfo info{};
        info.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        info.depthTestEnable  = depthTestEnable;
        info.depthWriteEnable = depthWriteEnable;
        info.depthCompareOp   = depthCompareOp;
        // info.back.compareOp = VK_COMPARE_OP_ALWAYS;
        return info;
    }

    VkPipelineViewportStateCreateInfo initPipelineViewportStateCreateInfo(
        uint32_t viewportCount,
        VkViewport* pViewports,
        uint32_t scissorCount,
        VkRect2D* pScissors,
        VkPipelineViewportStateCreateFlags flags) {
        VkPipelineViewportStateCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        info.viewportCount = viewportCount;
        info.pViewports    = pViewports;
        info.scissorCount  = scissorCount;
        info.pScissors     = pScissors;
        info.flags         = flags;
        return info;
    }

    VkPipelineMultisampleStateCreateInfo initPipelineMultisampleStateCreateInfo(
        VkSampleCountFlagBits rasterizationSamples,
        VkPipelineMultisampleStateCreateFlags flags) {
        VkPipelineMultisampleStateCreateInfo info{};
        info.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        info.rasterizationSamples = rasterizationSamples;
        info.minSampleShading     = 1.0f;
        info.flags                = flags;
        return info;
    }

    VkPipelineLayoutCreateInfo initPipelineLayoutCreateInfo(
        uint32_t layoutCount,
        VkDescriptorSetLayout* layouts,
        VkPipelineLayoutCreateFlags flags) {
        VkPipelineLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.setLayoutCount = layoutCount;
        info.pSetLayouts = layouts;
        info.flags = flags;
        return info;
    }

    VkDescriptorSetLayoutBinding initDescriptorSetLayoutBinding(
        uint32_t binding,
        VkDescriptorType type,
        VkPipelineStageFlags flags) {
        VkDescriptorSetLayoutBinding descSetLayoutBinding{};
        descSetLayoutBinding.descriptorType  = type;
        descSetLayoutBinding.descriptorCount = 1; // change manually later if needed
        descSetLayoutBinding.binding         = binding;
        descSetLayoutBinding.stageFlags      = flags;
        return descSetLayoutBinding;
    }

    VkDescriptorSetAllocateInfo initDescriptorSetAllocInfo(
        VkDescriptorPool pool,
        uint32_t count,
        VkDescriptorSetLayout* pDescSetLayouts) {
        VkDescriptorSetAllocateInfo descSetAllocInfo{};
        descSetAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descSetAllocInfo.descriptorPool     = pool;
        descSetAllocInfo.descriptorSetCount = count;
        descSetAllocInfo.pSetLayouts        = pDescSetLayouts;
        return descSetAllocInfo;
    }

    VkWriteDescriptorSet initWriteDescriptorSet(
        VkDescriptorSet dst,
        uint32_t binding,
        VkDescriptorType type,
        VkDescriptorBufferInfo* pBufferInfo) {
        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.descriptorType = type;
        writeDescriptorSet.dstSet = dst;
        writeDescriptorSet.dstBinding = binding;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.pBufferInfo = pBufferInfo;
        return writeDescriptorSet;
    }

    VkWriteDescriptorSet initWriteDescriptorSet(
        VkDescriptorSet dst,
        uint32_t binding,
        VkDescriptorType type,
        VkDescriptorImageInfo* pImageInfo) {
        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.descriptorType = type;
        writeDescriptorSet.dstSet = dst;
        writeDescriptorSet.dstBinding = binding;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.pImageInfo = pImageInfo;
        return writeDescriptorSet;
    }

    VkPipelineVertexInputStateCreateInfo initPipelineVertexInputStateCreateInfo(
        uint32_t bindingCount,
        VkVertexInputBindingDescription* pVertexBindingDescriptions,
        uint32_t attributesCount,
        VkVertexInputAttributeDescription* pVertexAttributesDescriptions,
        VkPipelineVertexInputStateCreateFlags flags) {
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.flags                           = flags;
        vertexInputInfo.vertexBindingDescriptionCount   = bindingCount;
        vertexInputInfo.pVertexBindingDescriptions      = pVertexBindingDescriptions;
        vertexInputInfo.vertexAttributeDescriptionCount = attributesCount;
        vertexInputInfo.pVertexAttributeDescriptions    = pVertexAttributesDescriptions;
        return vertexInputInfo;
    }

    VkPipelineShaderStageCreateInfo initPipelineShaderStageCreateInfo(
        VkShaderStageFlagBits stage,
        VkShaderModule& shader,
        const char* name) {
        VkPipelineShaderStageCreateInfo shaderStageInfo{};
        shaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfo.stage  = stage;
        shaderStageInfo.module = shader;
        shaderStageInfo.pName  = name;
        return shaderStageInfo;
    }
}