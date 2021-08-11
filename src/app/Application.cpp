///////////////////////////////////////////////////////
//
// Application definition
//
///////////////////////////////////////////////////////

// include class definition
#include <app/Application.h>
#include <app/AppConstants.h>

// include constants and utility
#include <utils/Utils.h>
#include <utils/Print.h>
#include <utils/Assert.h>

// transformations
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // because OpenGL uses depth range -1.0-1.0 and Vulkan uses 0.0-1.0
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <glm/gtx/string_cast.hpp>

#include <algorithm> // min, max
#include <fstream> // file (shader) loading
#include <cstdint> // UINT32_MAX
#include <set> // set for queues

// ImGui includes for a nice gui
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void Application::run() {
    initWindow();
    initVulkan();
    initImGui();
    mainLoop();
    cleanup();
}

void Application::initVulkan() {
    vkSetup.initSetup(window);

    // scene data
    camera = Camera({ 0.0f, 0.0f, 0.0f }, 2.0f, 10.0f);
    model.loadModel(MODEL_PATH);

    lights[0] = { {10.0f, 0.0f, 0.0f, 0.0f}, {200.0f, 200.0f, 200.0f}, 20.0f }; // pos, colour, radius
    lights[1] = { {-10.0f, 0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, 20.0f };
    lights[2] = { {0.0f, 0.0f, 10.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, 20.0f };
    lights[3] = { {0.0f, 0.0f, -10.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, 20.0f };

    createCommandPool(&renderCommandPool, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    createCommandPool(&imGuiCommandPool, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    createDescriptorSetLayout();

    swapChain.initSwapChain(&vkSetup, &model, &descriptorSetLayout);
    frameBuffer.initFrameBuffer(&vkSetup, &swapChain, renderCommandPool);
    gBuffer.createGBuffer(&vkSetup, &swapChain, &descriptorSetLayout, &model, renderCommandPool);
    
    // textures
    const std::vector<Image>* textureImages = model.getMaterialTextureData(0);
    textures.resize(textureImages->size());

    for (size_t i = 0; i < textureImages->size(); i++) {
        textures[i].createTexture(&vkSetup, renderCommandPool, textureImages->data()[i] );
    }

    skybox.createSkybox(&vkSetup, renderCommandPool);

    // vertex buffer 
    const std::vector<Model::Vertex>* vBuffer = model.getVertexBuffer(0);
    VulkanBuffer::createDeviceLocalBuffer(&vkSetup, renderCommandPool, 
        Buffer{ (unsigned char*)vBuffer->data(), vBuffer->size() * sizeof(Model::Vertex) }, // vertex data as buffer
        &vertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // index buffer
    const std::vector<uint32_t>* iBuffer = model.getIndexBuffer(0);
    VulkanBuffer::createDeviceLocalBuffer(&vkSetup, renderCommandPool, 
        Buffer{ (unsigned char*)iBuffer->data(), iBuffer->size() * sizeof(uint32_t) }, // index data as buffer
        &indexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    createDescriptorPool();
    createDescriptorSets();

    renderCommandBuffers.resize(swapChain.images.size());
    imGuiCommandBuffers.resize(swapChain.images.size());
    offScreenCommandBuffers.resize(swapChain.images.size());

    createCommandBuffers(static_cast<uint32_t>(renderCommandBuffers.size()), renderCommandBuffers.data(), renderCommandPool);
    createCommandBuffers(static_cast<uint32_t>(imGuiCommandBuffers.size()), imGuiCommandBuffers.data(), imGuiCommandPool);
    createCommandBuffers(static_cast<uint32_t>(offScreenCommandBuffers.size()), offScreenCommandBuffers.data(), renderCommandPool);

    createSyncObjects();

    // record commands
    for (size_t i = 0; i < swapChain.images.size(); i++) { 
        buildOffscreenCommandBuffer(i); // offscreen gbuffer commands
        buildCompositionCommandBuffer(i); // final image composition
    }
}

void Application::recreateVulkanData() {
    static int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(vkSetup.device); // wait if in use by device

    // destroy old swap chain dependencies
    vkFreeCommandBuffers(vkSetup.device, renderCommandPool, static_cast<uint32_t>(renderCommandBuffers.size()), renderCommandBuffers.data());
    vkFreeCommandBuffers(vkSetup.device, imGuiCommandPool, static_cast<uint32_t>(imGuiCommandBuffers.size()), imGuiCommandBuffers.data());
    vkFreeCommandBuffers(vkSetup.device, renderCommandPool, static_cast<uint32_t>(offScreenCommandBuffers.size()), offScreenCommandBuffers.data());

    gBuffer.cleanupGBuffer();
    framebufferData.cleanupFrambufferData();
    swapChain.cleanupSwapChain();

    // create new swap chain etc...
    swapChain.initSwapChain(&vkSetup, &model, &descriptorSetLayout);
    framebufferData.initFramebufferData(&vkSetup, &swapChain, renderCommandPool);
    gBuffer.createGBuffer(&vkSetup, &swapChain, &descriptorSetLayout, &model, renderCommandPool);

    createDescriptorSets();

    createCommandBuffers(static_cast<uint32_t>(renderCommandBuffers.size()), renderCommandBuffers.data(), renderCommandPool);
    createCommandBuffers(static_cast<uint32_t>(imGuiCommandBuffers.size()), imGuiCommandBuffers.data(), imGuiCommandPool);
    createCommandBuffers(static_cast<uint32_t>(offScreenCommandBuffers.size()), offScreenCommandBuffers.data(), renderCommandPool);

    for (size_t i = 0; i < swapChain.images.size(); i++) {
        buildOffscreenCommandBuffer(i);
        buildCompositionCommandBuffer(i);
        buildGuiCommandBuffer(i);
    }

    // update ImGui aswell
    ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(swapChain.images.size()));
}

void Application::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance       = vkSetup.instance;
    init_info.PhysicalDevice = vkSetup.physicalDevice;
    init_info.Device         = vkSetup.device;
    init_info.QueueFamily    = utils::QueueFamilyIndices::findQueueFamilies(vkSetup.physicalDevice, vkSetup.surface).graphicsFamily.value();
    init_info.Queue          = vkSetup.graphicsQueue;
    init_info.PipelineCache  = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptorPool;
    init_info.Allocator      = nullptr;
    init_info.MinImageCount  = swapChain.supportDetails.capabilities.minImageCount + 1;
    init_info.ImageCount     = static_cast<uint32_t>(swapChain.images.size());

    // the imgui render pass
    ImGui_ImplVulkan_Init(&init_info, swapChain.imGuiRenderPass);

    uploadFonts();
}

void Application::uploadFonts() {
    VkCommandBuffer commandbuffer = utils::beginSingleTimeCommands(&vkSetup.device, imGuiCommandPool);
    ImGui_ImplVulkan_CreateFontsTexture(commandbuffer);
    utils::endSingleTimeCommands(&vkSetup.device, &vkSetup.graphicsQueue, &commandbuffer, &imGuiCommandPool);
}

void Application::initWindow() {
    glfwInit();

    // set parameters
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // initially for opengl, so tell it not to create opengl context

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Deferred Rendering Demo", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

// Descriptors

void Application::createDescriptorPool() {
    uint32_t swapChainImageCount = static_cast<uint32_t>(swapChain.images.size());
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       IMGUI_POOL_NUM }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = IMGUI_POOL_NUM * static_cast<uint32_t>(swapChain.images.size());
    poolInfo.poolSizeCount = static_cast<uint32_t>(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
    poolInfo.pPoolSizes    = poolSizes; // the descriptors

    if (vkCreateDescriptorPool(vkSetup.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void Application::createDescriptorSetLayout() {

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        // binding 0: vertex shader uniform buffer 
        utils::initDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
        // binding 1: model albedo texture / position texture
        utils::initDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        // binding 2: model metallic roughness / normal texture
        utils::initDescriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        // binding 3: albedo texture
        utils::initDescriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
        // binding 4: fragment shader uniform buffer 
        utils::initDescriptorSetLayoutBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    VkDescriptorSetLayoutCreateInfo layoutCreateInf{};
    layoutCreateInf.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInf.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
    layoutCreateInf.pBindings = setLayoutBindings.data();

    if (vkCreateDescriptorSetLayout(vkSetup.device, &layoutCreateInf, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void Application::createDescriptorSets() {
    std::vector<VkWriteDescriptorSet> writeDescriptorSets;

    std::vector<VkDescriptorSetLayout> layouts(swapChain.images.size(), descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo = utils::initDescriptorSetAllocInfo(descriptorPool, 1, layouts.data());

    // offscreen descriptor set
    if (vkAllocateDescriptorSets(vkSetup.device, &allocInfo, &offScreenDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    // offscreen uniform
    VkDescriptorBufferInfo offScreenUboInf{};
    offScreenUboInf.buffer = gBuffer.offScreenUniform.buffer;
    offScreenUboInf.offset = 0;
    offScreenUboInf.range  = sizeof(GBuffer::OffScreenUbo);
    
    // offscrene textures in scene
    std::vector<VkDescriptorImageInfo> offScreenTexDescriptors(textures.size());
    for (size_t i = 0; i < textures.size(); i++) {
        offScreenTexDescriptors[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        offScreenTexDescriptors[i].imageView   = textures[i].textureImageView;
        offScreenTexDescriptors[i].sampler     = textures[i].textureSampler;
    }

    writeDescriptorSets = {
        // binding 0: vertex shader uniform buffer 
        utils::initWriteDescriptorSet(offScreenDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &offScreenUboInf),
        // binding 1: model albedo texture 
        utils::initWriteDescriptorSet(offScreenDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &offScreenTexDescriptors[0]),
        // binding 2: model metallic roughness texture
        utils::initWriteDescriptorSet(offScreenDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &offScreenTexDescriptors[1])
    };

    vkUpdateDescriptorSets(vkSetup.device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    // skybox descriptor set
    if (vkAllocateDescriptorSets(vkSetup.device, &allocInfo, &skyboxDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    // skybox uniform
    VkDescriptorBufferInfo skyboxUboInf{};
    skyboxUboInf.buffer = skybox.uniformBuffer.buffer;
    skyboxUboInf.offset = 0;
    skyboxUboInf.range = sizeof(Skybox::UBO);

    // skybox texture
    VkDescriptorImageInfo skyboxTexDescriptor{};
    skyboxTexDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    skyboxTexDescriptor.imageView = skybox.skyboxImageView;
    skyboxTexDescriptor.sampler = skybox.skyboxSampler;

    writeDescriptorSets = {
        // binding 0: vertex shader uniform buffer 
        utils::initWriteDescriptorSet(skyboxDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &skyboxUboInf),
        // binding 1: skybox texture 
        utils::initWriteDescriptorSet(skyboxDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &skyboxTexDescriptor)
    };

    vkUpdateDescriptorSets(vkSetup.device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    // composition descriptor sets
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    compositionDescriptorSets.resize(layouts.size());

    if (vkAllocateDescriptorSets(vkSetup.device, &allocInfo, compositionDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    // image descriptors for gBuffer color attachments
    VkDescriptorImageInfo texDescriptorPosition{};
    texDescriptorPosition.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    texDescriptorPosition.imageView = gBuffer.attachments["position"].imageView;
    texDescriptorPosition.sampler = gBuffer.colourSampler;

    VkDescriptorImageInfo texDescriptorNormal{};
    texDescriptorNormal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    texDescriptorNormal.imageView = gBuffer.attachments["normal"].imageView;
    texDescriptorNormal.sampler = gBuffer.colourSampler;

    VkDescriptorImageInfo texDescriptorAlbedo{};
    texDescriptorAlbedo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    texDescriptorAlbedo.imageView = gBuffer.attachments["albedo"].imageView;
    texDescriptorAlbedo.sampler = gBuffer.colourSampler;

    for (size_t i = 0; i < compositionDescriptorSets.size(); i++) {
        // forward rendering uniform buffer
        VkDescriptorBufferInfo compositionUboInf{};
        compositionUboInf.buffer = gBuffer.compositionUniforms.buffer;
        compositionUboInf.offset = sizeof(GBuffer::CompositionUBO) * i;
        compositionUboInf.range  = sizeof(GBuffer::CompositionUBO);

        // offscreen descriptor writes
        writeDescriptorSets = {
            // binding 1: position texture target 
            utils::initWriteDescriptorSet(compositionDescriptorSets[i], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &texDescriptorPosition),
            // binding 2: normal texture target
            utils::initWriteDescriptorSet(compositionDescriptorSets[i], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &texDescriptorNormal),
            // binding 3: albedo texture target
            utils::initWriteDescriptorSet(compositionDescriptorSets[i], 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &texDescriptorAlbedo),
            // binding 4: fragment shader uniform
            utils::initWriteDescriptorSet(compositionDescriptorSets[i], 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &compositionUboInf),
        };

        // update according to the configuration
        vkUpdateDescriptorSets(vkSetup.device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }
}

// Command buffers

void Application::createCommandPool(VkCommandPool* commandPool, VkCommandPoolCreateFlags flags) {
    utils::QueueFamilyIndices queueFamilyIndices = 
        utils::QueueFamilyIndices::findQueueFamilies(vkSetup.physicalDevice, vkSetup.surface);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(); // the queue to submit to
    poolInfo.flags            = flags;

    if (vkCreateCommandPool(vkSetup.device, &poolInfo, nullptr, commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void Application::createCommandBuffers(uint32_t count, VkCommandBuffer* commandBuffers, VkCommandPool& commandPool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY; 
    allocInfo.commandBufferCount = count;

    if (vkAllocateCommandBuffers(vkSetup.device, &allocInfo, commandBuffers) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void Application::buildCompositionCommandBuffer(size_t cmdBufferIndex) {
    VkCommandBufferBeginInfo commandBufferBeginInfo = utils::initCommandBufferBeginInfo();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color           = { 0.0f, 0.0f, 0.0f, 1.0f };
    clearValues[1].depthStencil    = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass        = swapChain.renderPass;
    renderPassBeginInfo.framebuffer       = framebufferData.framebuffers[cmdBufferIndex];
    renderPassBeginInfo.renderArea.offset = { 0, 0 };
    renderPassBeginInfo.renderArea.extent = swapChain.extent;
    renderPassBeginInfo.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues      = clearValues.data();

    // implicitly resets cmd buffer
    if (vkBeginCommandBuffer(renderCommandBuffers[cmdBufferIndex], &commandBufferBeginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
    
    vkCmdBeginRenderPass(renderCommandBuffers[cmdBufferIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(renderCommandBuffers[cmdBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gBuffer.deferredPipeline);
    vkCmdBindDescriptorSets(renderCommandBuffers[cmdBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, 
        swapChain.pipelineLayout, 0, 1, &compositionDescriptorSets[cmdBufferIndex], 0, nullptr);
    // draw a single triangle
    vkCmdDraw(renderCommandBuffers[cmdBufferIndex], 3, 1, 0, 0);
    vkCmdEndRenderPass(renderCommandBuffers[cmdBufferIndex]);

    if (vkEndCommandBuffer(renderCommandBuffers[cmdBufferIndex]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void Application::buildGuiCommandBuffer(size_t cmdBufferIndex) {
    VkCommandBufferBeginInfo commandbufferInfo = utils::initCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    if (vkBeginCommandBuffer(imGuiCommandBuffers[cmdBufferIndex], &commandbufferInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkClearValue clearValue{}; 
    clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f }; // completely opaque clear value

    // begin the render pass
    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass               = swapChain.imGuiRenderPass;
    renderPassBeginInfo.framebuffer              = framebufferData.imGuiFramebuffers[cmdBufferIndex];
    renderPassBeginInfo.renderArea.extent.width  = swapChain.extent.width;
    renderPassBeginInfo.renderArea.extent.height = swapChain.extent.height;
    renderPassBeginInfo.clearValueCount          = 1;
    renderPassBeginInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(imGuiCommandBuffers[cmdBufferIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), imGuiCommandBuffers[cmdBufferIndex]); // ends imgui render
    vkCmdEndRenderPass(imGuiCommandBuffers[cmdBufferIndex]);

    if (vkEndCommandBuffer(imGuiCommandBuffers[cmdBufferIndex]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record ImGui command buffer!");
    }
}

void Application::buildOffscreenCommandBuffer(size_t cmdBufferIndex) {
    VkCommandBufferBeginInfo commandBufferBeginInfo = utils::initCommandBufferBeginInfo();

    // Clear values for all attachments written in the fragment shader
    std::array<VkClearValue, 4> clearValues{};
    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
    clearValues[1].color = { 0.0f, 0.0f, 0.0f, 0.0f };
    clearValues[2].color = { 0.0f, 0.0f, 0.0f, 0.0f };
    clearValues[3].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass        = gBuffer.deferredRenderPass;
    renderPassBeginInfo.framebuffer       = gBuffer.deferredFrameBuffer;
    renderPassBeginInfo.renderArea.extent = gBuffer.extent;
    renderPassBeginInfo.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues      = clearValues.data();

    // implicitly resets cmd buffer
    if (vkBeginCommandBuffer(offScreenCommandBuffers[cmdBufferIndex], &commandBufferBeginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    vkCmdBeginRenderPass(offScreenCommandBuffers[cmdBufferIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // scene pipeline
    vkCmdBindPipeline(offScreenCommandBuffers[cmdBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gBuffer.offScreenPipeline);
    vkCmdBindDescriptorSets(offScreenCommandBuffers[cmdBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gBuffer.layout, 0, 1,
        &offScreenDescriptorSet, 0, nullptr);
    VkDeviceSize offset = 0; // offset into vertex buffer
    vkCmdBindVertexBuffers(offScreenCommandBuffers[cmdBufferIndex], 0, 1, &vertexBuffer.buffer, &offset);
    vkCmdBindIndexBuffer(offScreenCommandBuffers[cmdBufferIndex], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(offScreenCommandBuffers[cmdBufferIndex], model.getNumIndices(0), 1, 0, 0, 0);

    // skybox pipeline
    vkCmdBindPipeline(offScreenCommandBuffers[cmdBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gBuffer.skyboxPipeline);
    vkCmdBindDescriptorSets(offScreenCommandBuffers[cmdBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gBuffer.layout, 0, 1,
        &skyboxDescriptorSet, 0, nullptr);
    vkCmdBindVertexBuffers(offScreenCommandBuffers[cmdBufferIndex], 0, 1, &skybox.vertexBuffer.buffer, &offset);
    vkCmdDraw(offScreenCommandBuffers[cmdBufferIndex], 36, 1, 0, 0);

    vkCmdEndRenderPass(offScreenCommandBuffers[cmdBufferIndex]);

    if (vkEndCommandBuffer(offScreenCommandBuffers[cmdBufferIndex]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

// Handling window resize events

void Application::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    // pointer to this application class obtained from glfw, it doesnt know that it is a Application but we do so we can cast to it
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    // and set the resize flag to true
    app->framebufferResized = true;
}

// Synchronisation

void Application::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    offScreenSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapChain.images.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // initialise in the signaled state

    // simply loop over each frame and create semaphores for them
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vkSetup.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vkSetup.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vkSetup.device, &semaphoreInfo, nullptr, &offScreenSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
        if (vkCreateFence(vkSetup.device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create fences!");
        }
    }
}

//
// Main loop 
//

void Application::mainLoop() {
    prevTime = std::chrono::high_resolution_clock::now();
    
    // loop keeps window open
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        // get the time before the drawing frame
        currTime = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currTime - prevTime).count();

        if (processKeyInput() == 0)
            break;

        // sets the current GUI
        setGUI();

        // draw the frame
        drawFrame();

        prevTime = currTime;
    }
    vkDeviceWaitIdle(vkSetup.device);
}

// Frame drawing, GUI setting and UI

void Application::drawFrame() {
    // will acquire an image from swap chain, exec commands in command buffer with images as attachments in the 
    // frameBuffer return the image to the swap buffer. These tasks are started simultaneously but executed 
    // asynchronously. However we want these to occur in sequence because each relies on the previous task success
    // For syncing we can use semaphores or fences and coordinate operations by having one operation signal another 
    // and another wait for a fence or semaphore to go from unsignaled to signaled.
    // We can access fence state with vkWaitForFences but not semaphores.
    // Fences are mainly for syncing app with rendering operations, used here to synchronise the frame rate.
    // Semaphores are for syncing operations within or across cmd queues. 
    // We want to sync queue operations to draw cmds and presentation, and we want to make sure the offscreen cmds
    // have finished before the final image composition using semaphores. 
    /*************************************************************************************************************/

    // previous frame finished will fence
    vkWaitForFences(vkSetup.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(vkSetup.device, swapChain.swapChain, UINT64_MAX, 
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex); 

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateVulkanData();
        return; // return to acquire the image again
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) { 
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) { // Check if a previous frame is using this image
        vkWaitForFences(vkSetup.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }

    imagesInFlight[imageIndex] = inFlightFences[currentFrame]; // set image as in use by current frame

    updateUniformBuffers(imageIndex);

    buildGuiCommandBuffer(imageIndex);

    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = &waitStages;

    // offscreen rendering
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &imageAvailableSemaphores[currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &offScreenSemaphores[currentFrame];

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &offScreenCommandBuffers[currentFrame];

    if (vkQueueSubmit(vkSetup.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    // scene and Gui rendering
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &offScreenSemaphores[currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &renderFinishedSemaphores[currentFrame];

    std::array<VkCommandBuffer, 2> submitCommandBuffers = { renderCommandBuffers[imageIndex], imGuiCommandBuffers[imageIndex] };
    submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
    submitInfo.pCommandBuffers    = submitCommandBuffers.data();

    // reset the fence so fence blocks when submitting 
    vkResetFences(vkSetup.device, 1, &inFlightFences[currentFrame]); 

    // submit the command buffer to the graphics queue, takes an array of submitinfo when work load is much larger
    // last param is a fence, which is signaled when the cmd buffer finishes executing and is used to inform that the frame has finished
    // being rendered (the commands were all executed). The next frame can start rendering!
    if (vkQueueSubmit(vkSetup.graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    // submitting the result back to the swap chain to have it shown onto the screen
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &renderFinishedSemaphores[currentFrame];

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains    = &swapChain.swapChain;
    presentInfo.pImageIndices  = &imageIndex;

    // submit request to put image from the swap chain to the presentation queue
    result = vkQueuePresentKHR(vkSetup.presentQueue, &presentInfo);

    // check presentation queue can accept the image and any resize
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateVulkanData();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    // increment current frame 
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::setGUI() {
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame(); // empty
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const char* attachments[] = { "composition", "position", "normal", "albedo", "depth" };

    ImGui::Begin("Options", nullptr, ImGuiWindowFlags_NoMove);
    ImGui::BulletText("Transforms:");
    ImGui::SliderFloat3("translate", &translate[0], -2.0f, 2.0f);
    ImGui::SliderFloat3("rotate", &rotate[0], -180.0f, 180.0f);
    ImGui::SliderFloat("Scale:", &scale, 0.0f, 1.0f);
    ImGui::BulletText("Attachments:");
    ImGui::SameLine();
    ImGui::Combo("", &attachmentNum, attachments, SizeofArray(attachments));
    ImGui::End();
}

// Uniforms

void Application::updateUniformBuffers(uint32_t currentImage) {
    // offscreen ubo
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), swapChain.extent.width / (float)swapChain.extent.height, 0.1f, 10.0f);
    proj[1][1] *= -1.0f; // y coordinates inverted, Vulkan origin top left vs OpenGL bottom left

    glm::mat4 model = glm::translate(glm::mat4(1.0f), translate);
    model = glm::scale(model, glm::vec3(scale, scale, scale));
    glm::mat4 rotateZ(1.0f);
    rotateZ[0][0] *= -1.0f;
    rotateZ[1][1] *= -1.0f;
    model *= rotateZ * glm::toMat4(glm::quat(glm::radians(rotate)));

    GBuffer::OffScreenUbo offscreenUbo{};
    offscreenUbo.model = model;
    offscreenUbo.view = camera.getViewMatrix();
    offscreenUbo.projection = proj;
    offscreenUbo.normal = glm::transpose(glm::inverse(glm::mat3(model)));

    gBuffer.updateOffScreenUniformBuffer(offscreenUbo);

    // skybox ubo
    Skybox::UBO skyboxUbo{};
    skyboxUbo.view = glm::mat4(glm::mat3(camera.getViewMatrix()));
    skyboxUbo.projection = proj;

    skybox.updateSkyboxUniformBuffer(skyboxUbo);

    // composition ubo
    GBuffer::CompositionUBO compositionUbo = {};
    compositionUbo.guiData = { camera.position, attachmentNum };
    compositionUbo.lights[0] = lights[0]; // pos, colour, radius
    compositionUbo.lights[1] = lights[1];
    compositionUbo.lights[2] = lights[2];
    compositionUbo.lights[3] = lights[3];
    gBuffer.updateCompositionUniformBuffer(currentImage, compositionUbo);
}

int Application::processKeyInput() {
    // special case return 0 to exit the program
    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
        return 0;

    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
        // up/down 
        if (glfwGetKey(window, GLFW_KEY_UP))
            camera.processInput(CameraMovement::Upward, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_DOWN))
            camera.processInput(CameraMovement::Downward, deltaTime);
    }
    else {
        // front/back
        if (glfwGetKey(window, GLFW_KEY_UP))
            camera.processInput(CameraMovement::Forward, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_DOWN))
            camera.processInput(CameraMovement::Backward, deltaTime);
    }

    // left/right
    if (glfwGetKey(window, GLFW_KEY_LEFT))
        camera.processInput(CameraMovement::Left, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_RIGHT))
        camera.processInput(CameraMovement::Right, deltaTime);

    // pitch up/down
    if (glfwGetKey(window, GLFW_KEY_W))
        camera.processInput(CameraMovement::PitchUp, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S))
        camera.processInput(CameraMovement::PitchDown, deltaTime);

    // yaw left/right
    if (glfwGetKey(window, GLFW_KEY_A))
        camera.processInput(CameraMovement::YawLeft, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D))
        camera.processInput(CameraMovement::YawRight, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_Q))
        camera.processInput(CameraMovement::RollLeft, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E))
        camera.processInput(CameraMovement::RollRight, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_SPACE)) {
        camera.orientation.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
        camera.position = { 0.0f, 0.0f, 3.0f };
    }

    // other wise just return true
    return 1;
}

void Application::processMouseInput(glm::dvec2& curr) {
    // https://learnopengl.com/Getting-started/Camera

    if (firstMouse) {
        prevMouse = curr;
        firstMouse = false;
    }
    glm::dvec2 deltaMouse = curr - prevMouse;

    double sensitivity = 15;
    deltaMouse *= sensitivity;

    // offset dictates the amount we rotate by 
    //camera.yaw   = static_cast<float>(deltaMouse.x);
    //camera.pitch = static_cast<float>(deltaMouse.y);

    // pass the angles to the camera
    //camera.orientation.applyRotation(WORLD_UP, glm::radians(camera.yaw));
    //camera.orientation.applyRotation(WORLD_RIGHT, glm::radians(camera.pitch));
}

//
// Cleanup
//

void Application::cleanup() {
    // destroy the imgui context when the program ends
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    for (auto& texture : textures) {
        texture.cleanupTexture();
    }

    skybox.cleanupSkybox();

    // destroy whatever is dependent on the old swap chain
    vkFreeCommandBuffers(vkSetup.device, renderCommandPool, static_cast<uint32_t>(renderCommandBuffers.size()), renderCommandBuffers.data());
    vkFreeCommandBuffers(vkSetup.device, imGuiCommandPool, static_cast<uint32_t>(imGuiCommandBuffers.size()), imGuiCommandBuffers.data());
    vkFreeCommandBuffers(vkSetup.device, renderCommandPool, static_cast<uint32_t>(offScreenCommandBuffers.size()), offScreenCommandBuffers.data());

    // call the function we created for destroying the swap chain and frame buffers
    // in the reverse order of their creation
    gBuffer.cleanupGBuffer();
    framebufferData.cleanupFrambufferData();
    swapChain.cleanupSwapChain();

    // cleanup the descriptor pools and descriptor set layouts
    vkDestroyDescriptorPool(vkSetup.device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(vkSetup.device, descriptorSetLayout, nullptr);

    // destroy the index and vertex buffers
    indexBuffer.cleanupBufferData(vkSetup.device);
    vertexBuffer.cleanupBufferData(vkSetup.device);

    // loop over each frame and destroy its semaphores and fences
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(vkSetup.device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(vkSetup.device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(vkSetup.device, offScreenSemaphores[i], nullptr);
        vkDestroyFence(vkSetup.device, inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(vkSetup.device, renderCommandPool, nullptr);
    vkDestroyCommandPool(vkSetup.device, imGuiCommandPool, nullptr);

    vkSetup.cleanupSetup();

    // destory the window
    glfwDestroyWindow(window);

    // terminate glfw
    glfwTerminate();
}

