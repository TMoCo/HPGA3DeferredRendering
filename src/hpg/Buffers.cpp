//
// Buffer class definition
//

#include <hpg/Buffers.h>

#include <utils/Utils.h>

void VulkanBuffer::cleanupBufferData(const VkDevice& device) {
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, memory, nullptr);
}

void VulkanBuffer::copyBufferToImage(const VulkanSetup* vkSetup, const VkCommandPool& renderCommandPool, VkBuffer buffer,
	VkImage image, const std::vector<VkBufferImageCopy>& regions) {
    // copying buffer to image
    VkCommandBuffer commandBuffer = utils::beginSingleTimeCommands(&vkSetup->device, renderCommandPool);
    // buffer to image copy operations are enqueued thus
    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        static_cast<uint32_t>(regions.size()), regions.data());
    utils::endSingleTimeCommands(&vkSetup->device, &vkSetup->graphicsQueue, &commandBuffer, &renderCommandPool);
}

void VulkanBuffer::createBuffer(const VulkanSetup* vkSetup, VulkanBuffer::CreateInfo* bufferCreateInfo) {
    // fill in the corresponding struct
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferCreateInfo->size; // allocate a buffer of the right size in bytes
    bufferInfo.usage = bufferCreateInfo->usage; // what the data in the buffer is used for
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // buffers can be owned by a single queue or shared between many
    // bufferInfo.flags = 0; // to configure sparse memory

    // attempt to create a buffer
    if (vkCreateBuffer(vkSetup->device, &bufferInfo, nullptr, &bufferCreateInfo->pVulkanBuffer->buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vertex buffer!");
    }

    // created a buffer, but haven't assigned any memory yet, also get the right memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vkSetup->device, bufferCreateInfo->pVulkanBuffer->buffer, &memRequirements);

    // allocate the memory for the buffer
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = utils::findMemoryType(&vkSetup->physicalDevice, memRequirements.memoryTypeBits, bufferCreateInfo->properties);

    // allocate memory for the buffer. In a real world application, not supposed to actually call vkAllocateMemory for every individual buffer. 
    // The maximum number of simultaneous memory allocations is limited by the maxMemoryAllocationCount physical device limit. The right way to 
    // allocate memory for large number of objects at the same time is to create a custom allocator that splits up a single allocation among many 
    // different objects by using the offset parameters seen in other functions
    if (vkAllocateMemory(vkSetup->device, &allocInfo, nullptr, &bufferCreateInfo->pVulkanBuffer->memory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }

    // associate memory with buffer
    vkBindBufferMemory(vkSetup->device, bufferCreateInfo->pVulkanBuffer->buffer, bufferCreateInfo->pVulkanBuffer->memory, 0);
}

void VulkanBuffer::copyBuffer(const VulkanSetup* vkSetup, const VkCommandPool& commandPool, VulkanBuffer::CopyInfo* bufferCopyInfo) {
    // memory transfer operations are executed using command buffers, like drawing commands. We need to allocate a temporary command buffer
    // could use a command pool for these short lived operations using the flag VK_COMMAND_POOL_CREATE_TRANSIENT_BIT 
    VkCommandBuffer commandBuffer = utils::beginSingleTimeCommands(&vkSetup->device, commandPool);
    vkCmdCopyBuffer(commandBuffer, *bufferCopyInfo->pSrc, *bufferCopyInfo->pDst, 1, &bufferCopyInfo->copyRegion);
    utils::endSingleTimeCommands(&vkSetup->device, &vkSetup->graphicsQueue, &commandBuffer, &commandPool);
}

void VulkanBuffer::createDeviceLocalBuffer(const VulkanSetup* vkSetup, const VkCommandPool& commandPool, const Buffer& buffer, VulkanBuffer* vkBuffer, VkBufferUsageFlagBits usage) {
    VulkanBuffer stagingBuffer;

    VulkanBuffer::CreateInfo createInfo{};
    createInfo.size = buffer.size;
    createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    createInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createInfo.pVulkanBuffer = &stagingBuffer;

    VulkanBuffer::createBuffer(vkSetup, &createInfo);

    void* data;
    vkMapMemory(vkSetup->device, stagingBuffer.memory, 0, buffer.size, 0, &data);
    memcpy(data, buffer.data, buffer.size);
    vkUnmapMemory(vkSetup->device, stagingBuffer.memory);

    // different usage bit flag VK_BUFFER_USAGE_INDEX_BUFFER_BIT instead of VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    createInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage;
    createInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    createInfo.pVulkanBuffer = vkBuffer;

    VulkanBuffer::createBuffer(vkSetup, &createInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = buffer.size;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;

    VulkanBuffer::CopyInfo copyInfo{};
    copyInfo.pSrc = &stagingBuffer.buffer;
    copyInfo.pDst = &vkBuffer->buffer;
    copyInfo.copyRegion = copyRegion;

    VulkanBuffer::copyBuffer(vkSetup, commandPool, &copyInfo);

    stagingBuffer.cleanupBufferData(vkSetup->device);
}