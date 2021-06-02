///////////////////////////////////////////////////////
//
// Application definition
//
///////////////////////////////////////////////////////

// include class definition
#include <app/Application.h>
#include <app/AppConstants.h>

// include constants
#include <utils/Utils.h>
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
    // initialise a glfw window
    initWindow();

    // initialise vulkan
    initVulkan();

    // initialise imgui
    initImGui();

    // run the main loop
    mainLoop();

    // clean up before exiting
    cleanup();
}

// Init 

void Application::initVulkan() {
    // create the vulkan core
    vkSetup.initSetup(window);

    // scene data
    camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f), 50.0f, 50.0f);

    // load the model
    model.loadModel(MODEL_PATH);

    // get texture data from model 
    const std::vector<Image>* textureImages = model.getMaterialTextureData(0);
    textures.resize(textureImages->size());

    // create descriptor layout and command pools (immutable over app lifetime)
    createDescriptorSetLayout();
    createCommandPool(&renderCommandPool, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    createCommandPool(&imGuiCommandPool, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // swap chain and frame buffers
    swapChain.initSwapChain(&vkSetup, &model, &descriptorSetLayout);
    framebufferData.initFramebufferData(&vkSetup, &swapChain, renderCommandPool);
    gBuffer.createGBuffer(&vkSetup, &swapChain, renderCommandPool);
    
    // textures
    auto texture = textures.begin(); // iterator to first texture element
    for (size_t i = 0; i < textureImages->size(); i++) {
        texture->createTexture(&vkSetup, renderCommandPool, textureImages->data()[i] );
        texture++; // increment iter
    }

    // vertex buffer 
    const std::vector<Model::Vertex>* vBuffer = model.getVertexBuffer(0);
    VulkanBuffer::createDeviceLocalBuffer(&vkSetup, renderCommandPool, Buffer{ (unsigned char*)vBuffer->data(), vBuffer->size() * sizeof(Model::Vertex) }, &vertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // index buffer
    const std::vector<uint32_t>* iBuffer = model.getIndexBuffer(0);
    VulkanBuffer::createDeviceLocalBuffer(&vkSetup, renderCommandPool, Buffer{ (unsigned char*)iBuffer->data(), iBuffer->size() * sizeof(uint32_t) }, &indexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    // uniform buffer  
    VulkanBuffer::createUniformBuffer<GBuffer::UBO>(&vkSetup, swapChain.images.size(), &uniforms, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // descriptor sets
    createDescriptorPool();
    createDescriptorSets();

    // command buffers
    createCommandBuffers(&renderCommandBuffers, renderCommandPool);
    createCommandBuffers(&imGuiCommandBuffers, imGuiCommandPool);

    // setup synchronisation
    createSyncObjects();

    // record the command buffer in every image index (commands do not change from one frame to next)
    for (size_t i = 0; i < swapChain.images.size(); i++) {
        recordGeometryCommandBuffer(i);
    }
}

void Application::recreateVulkanData() {
    static int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    // wait before destroying if in use by the device
    vkDeviceWaitIdle(vkSetup.device);

    // destroy whatever is dependent on the old swap chain, starting with the command buffers
    vkFreeCommandBuffers(vkSetup.device, renderCommandPool, static_cast<uint32_t>(renderCommandBuffers.size()), renderCommandBuffers.data());
    vkFreeCommandBuffers(vkSetup.device, imGuiCommandPool, static_cast<uint32_t>(imGuiCommandBuffers.size()), imGuiCommandBuffers.data());

    uniforms.cleanupBufferData(vkSetup.device);

    gBuffer.cleanupGBuffer();
    framebufferData.cleanupFrambufferData();
    swapChain.cleanupSwapChain();

    // recreate
    swapChain.initSwapChain(&vkSetup, &model, &descriptorSetLayout);
    framebufferData.initFramebufferData(&vkSetup, &swapChain, renderCommandPool);
    gBuffer.createGBuffer(&vkSetup, &swapChain, renderCommandPool);

    VulkanBuffer::createUniformBuffer<GBuffer::UBO>(&vkSetup, swapChain.images.size(), &uniforms, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    createDescriptorSets();

    createCommandBuffers(&renderCommandBuffers, renderCommandPool);
    createCommandBuffers(&imGuiCommandBuffers, imGuiCommandPool);

    for (size_t i = 0; i < swapChain.images.size(); i++) {
        recordGeometryCommandBuffer(i);
    }

    // update ImGui aswell
    ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(swapChain.images.size()));
}

void Application::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

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

void Application::createDescriptorSetLayout() {
    // A uniform block
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.binding            = 0;
    uboLayoutBinding.descriptorCount    = 1;
    uboLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; //
    uboLayoutBinding.pImmutableSamplers = nullptr;

    // same as above but for a texture sampler rather than for uniforms
    VkDescriptorSetLayoutBinding samplersLayoutBinding{};
    samplersLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplersLayoutBinding.binding            = 1;
    samplersLayoutBinding.descriptorCount    = static_cast<uint32_t>(textures.size()); // nb of textures
    samplersLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplersLayoutBinding.pImmutableSamplers = nullptr;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplersLayoutBinding }; // { terrainUboLayoutBinding, terrainHeightSamplerLayoutBinding, terrainTextureSamplerLayoutBinding };

    VkDescriptorSetLayoutCreateInfo layoutCreateInf{};
    layoutCreateInf.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInf.bindingCount = static_cast<uint32_t>(bindings.size()); // number of bindings
    layoutCreateInf.pBindings    = bindings.data(); // pointer to the bindings

    if (vkCreateDescriptorSetLayout(vkSetup.device, &layoutCreateInf, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    // Airplane descriptor layout
    /*
    VkDescriptorSetLayoutBinding airplaneUboLayoutBinding{};
    airplaneUboLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    airplaneUboLayoutBinding.binding            = 0; // the first descriptor
    airplaneUboLayoutBinding.descriptorCount    = 1; // single uniform buffer object so just 1, could be used to specify a transform for each bone in a skeletal animation
    airplaneUboLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // in which shader stage is the descriptor going to be referenced
    airplaneUboLayoutBinding.pImmutableSamplers = nullptr; // relevant to image sampling related descriptors


    // same as above but for a texture sampler rather than for uniforms
    VkDescriptorSetLayoutBinding airplaneSamplerLayoutBinding{};
    airplaneSamplerLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // this is a sampler descriptor
    airplaneSamplerLayoutBinding.binding            = 1; // the second descriptor
    airplaneSamplerLayoutBinding.descriptorCount    = 1;
    airplaneSamplerLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT; ;// the shader stage we wan the descriptor to be used in, ie the fragment shader stage
    // can use the texture sampler in the vertex stage as part of a height map to deform the vertices in a grid
    airplaneSamplerLayoutBinding.pImmutableSamplers = nullptr;

    // put the descriptors in an array
    std::array<VkDescriptorSetLayoutBinding, 0> airplaneBindings = {};// { airplaneUboLayoutBinding, airplaneSamplerLayoutBinding };
    
    // descriptor set bindings combined into a descriptor set layour object, created the same way as other vk objects by filling a struct in
    VkDescriptorSetLayoutCreateInfo airplaneLayoutInfo{};
    airplaneLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    airplaneLayoutInfo.bindingCount = static_cast<uint32_t>(airplaneBindings.size());; // number of bindings
    airplaneLayoutInfo.pBindings    = airplaneBindings.data(); // pointer to the bindings

    if (vkCreateDescriptorSetLayout(vkSetup.device, &airplaneLayoutInfo, nullptr, &descriptorSetLayouts[1]) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
    */
}

void Application::createDescriptorPool() {
    uint32_t swapChainImageCount = static_cast<uint32_t>(swapChain.images.size());
    VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_POOL_NUM + swapChainImageCount },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, IMGUI_POOL_NUM + swapChainImageCount },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, IMGUI_POOL_NUM },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, IMGUI_POOL_NUM }
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

void Application::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(swapChain.images.size(), descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChain.images.size());
    allocInfo.pSetLayouts        = layouts.data();

    descriptorSets.resize(swapChain.images.size());

    if (vkAllocateDescriptorSets(vkSetup.device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    // configure sets
    for (size_t i = 0; i < swapChain.images.size(); i++) {
        VkDescriptorBufferInfo uboBufferInf{};
        uboBufferInf.buffer = uniforms.buffer;
        uboBufferInf.offset = sizeof(GBuffer::UBO) * i;
        uboBufferInf.range  = sizeof(GBuffer::UBO);

        std::vector<VkDescriptorImageInfo> texturesImageInfo(textures.size());

        for (uint32_t i = 0; i < textures.size(); ++i)
        {
            texturesImageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            texturesImageInfo[i].imageView = textures[i].textureImageView;
            texturesImageInfo[i].sampler = textures[i].textureSampler;
        }

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{}; // add more descriptors here...

        // uniform buffer
        descriptorWrites[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet           = descriptorSets[i]; // wich set to update
        descriptorWrites[0].dstBinding       = 0; // uniform buffer has binding 0
        descriptorWrites[0].dstArrayElement  = 0; // descriptors can be arrays, only one element so first index
        descriptorWrites[0].descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount  = 1; // can update multiple descriptors at once starting at dstArrayElement, descriptorCount specifies how many elements
        descriptorWrites[0].pBufferInfo      = &uboBufferInf;

        // textures
        descriptorWrites[1].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet           = descriptorSets[i]; // wich set to update
        descriptorWrites[1].dstBinding       = 1; // uniform buffer has binding 0
        descriptorWrites[1].dstArrayElement  = 0; // descriptors can be arrays, only one element so first index
        descriptorWrites[1].descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount  = static_cast<uint32_t>(texturesImageInfo.size()); // can update multiple descriptors at once starting at dstArrayElement, descriptorCount specifies how many elements
        descriptorWrites[1].pImageInfo       = texturesImageInfo.data();

        // update according to the configuration
        vkUpdateDescriptorSets(vkSetup.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

// Uniforms

void Application::updateUniformBuffers(uint32_t currentImage) {
    glm::mat4 view = camera.getViewMatrix();

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), swapChain.extent.width / (float)swapChain.extent.height, 0.1f, 32000.0f);
    proj[1][1] *= -1; // y coordinates inverted, Vulkan origin top left vs OpenGL bottom left
    
    glm::mat4 model = glm::translate(glm::mat4(1.0f), translate);
    model = glm::scale(model, glm::vec3(scale, scale, scale));
    model *= glm::toMat4(glm::quat(glm::radians(rotate)));

    GBuffer::UBO ubo = { model, view, proj };
    gBuffer.updateUniformBuffer(ubo);

    void* data;
    vkMapMemory(vkSetup.device, uniforms.memory, sizeof(ubo) * currentImage, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(vkSetup.device, uniforms.memory);
}

// Buffer utils

std::vector<VkBuffer> Application::unwrapVkBuffers(const std::vector<VulkanBuffer>& vkBuffers) {
    m_assert(vkBuffers.size() > 0, "There must be at least one buffer to extract...");
    std::vector<VkBuffer> unwrapped(vkBuffers.size());

    for (size_t i = 0; i < vkBuffers.size(); i++) {
        unwrapped[i] = vkBuffers[i].buffer;
    }

    return unwrapped;
}

// Command buffers

void Application::createCommandPool(VkCommandPool* commandPool, VkCommandPoolCreateFlags flags) {
    utils::QueueFamilyIndices queueFamilyIndices = utils::QueueFamilyIndices::findQueueFamilies(vkSetup.physicalDevice, vkSetup.surface);

    // command pool needs two parameters
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // the queue to submit to
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    // flag for command pool, influences how command buffers are rerecorded
    // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT -> rerecorded with new commands often
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT -> let command buffers be rerecorded individually rather than together
    poolInfo.flags = flags; // in our case, we only record at beginning of program so leave empty

    // and create the command pool, we therfore ave to destroy it explicitly in cleanup
    if (vkCreateCommandPool(vkSetup.device, &poolInfo, nullptr, commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void Application::createCommandBuffers(std::vector<VkCommandBuffer>* commandBuffers, VkCommandPool& commandPool) {
    // resize the command buffers container to the same size as the frame buffers container
    commandBuffers->resize(framebufferData.framebuffers.size());

    // create the struct
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; // identify the struct type
    allocInfo.commandPool = commandPool; // specify the command pool
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // level specifes is the buffers are primary or secondary
    // VK_COMMAND_BUFFER_LEVEL_PRIMARY -> can be submitted to a queue for exec but not called from other command buffers
    // VK_COMMAND_BUFFER_LEVEL_SECONDARY -> cannot be submitted directly, but can be called from primary command buffers
    allocInfo.commandBufferCount = (uint32_t)commandBuffers->size(); // the number of buffers to allocate

    if (vkAllocateCommandBuffers(vkSetup.device, &allocInfo, commandBuffers->data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void Application::recordGeometryCommandBuffer(size_t cmdBufferIndex) {
    // the following struct used as argument specifying details about the usage of specific command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = 0; // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // how we are going to use the command buffer
    beginInfo.pInheritanceInfo = nullptr; // relevant to secondary comnmand buffers

    // creating implicilty resets the command buffer if it was already recorded once, cannot append
    // commands to a buffer at a later time!
    if (vkBeginCommandBuffer(renderCommandBuffers[cmdBufferIndex], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // create a render pass, initialised with some params in the following struct
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = swapChain.renderPass; // handle to the render pass and the attachments to bind
    renderPassInfo.framebuffer       = framebufferData.framebuffers[cmdBufferIndex]; // the framebuffer created for each swapchain image view
    renderPassInfo.renderArea.offset = { 0, 0 }; // some offset for the render area
    // best performance if same size as attachment
    renderPassInfo.renderArea.extent = swapChain.extent; // size of the render area (where shaders load and stores occur, pixels outside are undefined)

    // because we used the VK_ATTACHMENT_LOAD_OP_CLEAR for load operations of the render pass, we need to set clear colours
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color           = { 0.0f, 0.0f, 0.0f, 1.0f }; // black with max opacity
    clearValues[1].depthStencil    = { 1.0f, 0 }; // initialise the depth value to far (1 in the range of 0 to 1)
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size()); // only use a single value
    renderPassInfo.pClearValues    = clearValues.data(); // the colour to use for clear operation

    // begin the render pass. All vkCmd functions are void, so error handling occurs at the end
    // first param for all cmd are the command buffer to record command to, second details the render pass we've provided
    vkCmdBeginRenderPass(renderCommandBuffers[cmdBufferIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    // final parameter controls how drawing commands within the render pass will be provided 
    
    VkDeviceSize offset = 0;

    // bind the graphics pipeline, second param determines if the object is a graphics or compute pipeline
    vkCmdBindPipeline(renderCommandBuffers[cmdBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, swapChain.pipeline);
    vkCmdBindVertexBuffers(renderCommandBuffers[cmdBufferIndex], 0, 1, &vertexBuffer.buffer, &offset);

    // bind the index buffer, can only have a single index buffer 
    vkCmdBindIndexBuffer(renderCommandBuffers[cmdBufferIndex], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    // bind the uniform descriptor sets
    vkCmdBindDescriptorSets(renderCommandBuffers[cmdBufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, swapChain.pipelineLayout, 0, 1, &descriptorSets[cmdBufferIndex], 0, nullptr);

    vkCmdDrawIndexed(renderCommandBuffers[cmdBufferIndex], model.getNumIndices(0), 1, 0, 0, 0);

    // end the render pass
    vkCmdEndRenderPass(renderCommandBuffers[cmdBufferIndex]);

    // we've finished recording, so end recording and check for errors
    if (vkEndCommandBuffer(renderCommandBuffers[cmdBufferIndex]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void Application::recordGuiCommandBuffer(size_t cmdBufferIndex) {
    // tell ImGui to render
    ImGui::Render();

    // start recording into a command buffer
    VkCommandBufferBeginInfo commandbufferInfo = {};
    commandbufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandbufferInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(imGuiCommandBuffers[cmdBufferIndex], &commandbufferInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // begin the render pass
    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = swapChain.imGuiRenderPass;
    renderPassBeginInfo.framebuffer = framebufferData.imGuiFramebuffers[cmdBufferIndex];
    renderPassBeginInfo.renderArea.extent.width = swapChain.extent.width;
    renderPassBeginInfo.renderArea.extent.height = swapChain.extent.height;
    renderPassBeginInfo.clearValueCount = 1;

    VkClearValue clearValue{ 0.0f, 0.0f, 0.0f, 0.0f }; // completely opaque clear value
    renderPassBeginInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(imGuiCommandBuffers[imageIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Record Imgui Draw Data and draw funcs into command buffer
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), imGuiCommandBuffers[cmdBufferIndex]);

    vkCmdEndRenderPass(imGuiCommandBuffers[cmdBufferIndex]);

    vkEndCommandBuffer(imGuiCommandBuffers[cmdBufferIndex]);
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
    // resize the semaphores to the number of simultaneous frames, each has its own semaphores
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapChain.images.size(), VK_NULL_HANDLE); // explicitly initialise the fences in this vector to no fence

    VkSemaphoreCreateInfo semaphoreInfo{};
    // only required field at the moment, may change in the future
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // initialise in the signaled state

    // simply loop over each frame and create semaphores for them
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // attempt to create the semaphors
        if (vkCreateSemaphore(vkSetup.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vkSetup.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(vkSetup.device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }
}

//
// Main loop 
//

void Application::mainLoop() {
    prevTime = std::chrono::high_resolution_clock::now();
    glfwGetCursorPos(window, &prevMouse.x, &prevMouse.y);
    
    // loop keeps window open
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        // get the time before the drawing frame
        deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - prevTime).count();

        glfwGetCursorPos(window, &currMouse.x, &currMouse.y);

        if (processKeyInput() == 0)
            break;

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            // get the current mouse position
            processMouseInput(currMouse);
        }
        else {
            firstMouse = true;
        }

        setGUI();
        // draw the frame
        drawFrame();

        prevTime = std::chrono::high_resolution_clock::now();
        glfwGetCursorPos(window, &prevMouse.x, &prevMouse.y);
    }
    vkDeviceWaitIdle(vkSetup.device);
}

// Frame drawing, GUI setting and UI

void Application::drawFrame() {
    // will acquire an image from swap chain, exec commands in command buffer with images as attachments in the frameBuffer
    // return the image to the swap buffer. These tasks are started simultaneously but executed asynchronously.
    // However we want these to occur in sequence because each relies on the previous task success
    // For syncing can use semaphores or fences and coordinate operations by having one op signal another
    // op and another operation wait for a fence or semaphor to go from unsignaled to signaled.
    // we can access fence state with vkWaitForFences and not semaphores.
    // fences are mainly for syncing app with rendering op, use here to synchronise the frame rate
    // semaphores are for syncing ops within or across cmd queues. We want to sync queue op to draw cmds and presentation so pref semaphores here

    // at the start of the frame, make sure that the previous frame has finished which will signal the fence
    vkWaitForFences(vkSetup.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // retrieve an image from the swap chain
    // swap chain is an extension so use the vk*KHR function
    VkResult result = vkAcquireNextImageKHR(vkSetup.device, swapChain.swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex); // params:
    // the logical device and the swap chain we want to restrieve image from
    // a timeout in ns. Using UINT64_MAX disables it
    // synchronisation objects, so a semaphore
    // handle to another sync object (which we don't use so nul handle)
    // variable to output the swap chain image that has become available

    // Vulkan tells us if the swap chain is out of date with this result value (the swap chain is incompatible with the surface, eg window resize)
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // if so then recreate the swap chain and try to acquire the image from the new swap chain
        recreateVulkanData();
        return; // return to acquire the image again
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) { // both values here are considered "success", even if partial, values
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // Check if a previous frame is using this image (i.e. there is its fence to wait on)
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(vkSetup.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }

    // Mark the image as now being in use by this frame
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    // update the unifrom buffer before submitting
    updateUniformBuffers(imageIndex);

    // record the GUI command buffers
    recordGuiCommandBuffer(imageIndex);

    // the two command buffers, for geometry and UI
    std::array<VkCommandBuffer, 2> submitCommandBuffers = { renderCommandBuffers[imageIndex], imGuiCommandBuffers[imageIndex] };

    // info needed to submit the command buffers
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    // which semaphores to wait on before execution begins
    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    // which stages of the pipeline to wait at (here at the stage where we write colours to the attachment)
    // we can in theory start work on vertex shader etc while image is not yet available
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages; // for each sempahore we provide a wait stage
    // which command buffer to submit to, submit the cmd buffer that binds the swap chain image we acquired as color attachment
    submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
    submitInfo.pCommandBuffers = submitCommandBuffers.data();

    // which semaphores to signal once the command buffer(s) has finished, we are using the renderFinishedSemaphore for that
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // reset the fence so that when submitting the commands to the graphics queue, the fence is set to block so no subsequent 
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
    // which semaphores to wait on 
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    // specify the swap chains to present image to and index of image for each swap chain
    VkSwapchainKHR swapChains[] = { swapChain.swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    // allows to specify an array of vKResults to check for every individual swap chain if presentation is succesful
    presentInfo.pResults = nullptr; // Optional

    // submit the request to put an image from the swap chain to the presentation queue
    result = vkQueuePresentKHR(vkSetup.presentQueue, &presentInfo);

    // similar to when acquiring the swap chain image, check that the presentation queue can accept the image, also check for resizing
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        // tell the swapchaindata to change whether to enable or disable the depth testing when recreating the pipeline
        framebufferResized = false;
        recreateVulkanData();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    // after the frame is drawn and presented, increment current frame count (% loops around)
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::setGUI() {
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame(); // empty
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Options", nullptr, ImGuiWindowFlags_NoMove);
    ImGui::BulletText("Transforms:");
    ImGui::SliderFloat3("translate", &translate[0], -2.0f, 2.0f);
    ImGui::SliderFloat3("rotate", &rotate[0], -180.0f, 180.0f);
    ImGui::SliderFloat("Scale:", &scale, 0.0f, 1.0f);
    ImGui::End();
}

int Application::processKeyInput() {
    // special case return 0 to exit the program
    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
        return 0;

    // debug camera on
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
        if (glfwGetKey(window, GLFW_KEY_W) || glfwGetKey(window, GLFW_KEY_UP))
            camera.processInput(CameraMovement::Upward, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) || glfwGetKey(window, GLFW_KEY_DOWN))
            camera.processInput(CameraMovement::Downward, deltaTime);
    }
    else {
        if (glfwGetKey(window, GLFW_KEY_W) || glfwGetKey(window, GLFW_KEY_UP))
            camera.processInput(CameraMovement::Forward, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) || glfwGetKey(window, GLFW_KEY_DOWN))
            camera.processInput(CameraMovement::Backward, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_A) || glfwGetKey(window, GLFW_KEY_LEFT))
        camera.processInput(CameraMovement::Left, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) || glfwGetKey(window, GLFW_KEY_RIGHT))
        camera.processInput(CameraMovement::Right, deltaTime);
    
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
    camera.yaw   = static_cast<float>(deltaMouse.x);
    camera.pitch = static_cast<float>(deltaMouse.y);

    // pass the angles to the camera
    camera.orientation.applyRotation(WORLD_UP, glm::radians(camera.yaw));
    camera.orientation.applyRotation(WORLD_RIGHT, glm::radians(camera.pitch));
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

    // destroy whatever is dependent on the old swap chain
    vkFreeCommandBuffers(vkSetup.device, renderCommandPool, static_cast<uint32_t>(renderCommandBuffers.size()), renderCommandBuffers.data());
    vkFreeCommandBuffers(vkSetup.device, imGuiCommandPool, static_cast<uint32_t>(imGuiCommandBuffers.size()), imGuiCommandBuffers.data());
    
    // also destroy the uniform buffers that worked with the swap chain
    uniforms.cleanupBufferData(vkSetup.device);

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

