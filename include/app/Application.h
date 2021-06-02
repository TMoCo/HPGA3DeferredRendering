///////////////////////////////////////////////////////
// Application class declaration
///////////////////////////////////////////////////////

//
// The main application class. Member variables include the vulkan data needed
// for rendering some basis scenes, scene data and GUI. Contains the main rendering
// loop and is instantiated in program main.
//

#ifndef TERRAIN_APPLICATION_H
#define TERRAIN_APPLICATION_H

#include <common/Model.h> // the model class
#include <common/Camera.h> // the camera struct

// classes for vulkan
#include <hpg/VulkanSetup.h> // include the vulkan setup class
#include <hpg/SwapChain.h> // the swap chain class
#include <hpg/FramebufferData.h> // the framebuffer data class
#include <hpg/GBuffer.h>
#include <hpg/Buffers.h> // the buffer class

// glfw window library
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// vectors, matrices ...
#include <glm/glm.hpp>

// reporting and propagating exceptions
#include <iostream> 
#include <stdexcept>
#include <vector> // very handy containers of objects
#include <array>
#include <string> // string for file name
#include <chrono> // time 

class Application {
public:
    void run();

private:
    //-Initialise all our data for rendering------------------------------//
    void initVulkan();
    void recreateVulkanData();

    //-Initialise Imgui data----------------------------------------------//
    void initImGui();
    void uploadFonts();

    //-Initialise GLFW window---------------------------------------------//
    void initWindow();

    //-Descriptor initialisation functions--------------------------------//
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();

    //-Update uniform buffer----------------------------------------------//
    void updateUniformBuffers(uint32_t currentImage);

    //-Extract buffers from wrappers--------------------------------------//
    std::vector<VkBuffer> unwrapVkBuffers(const std::vector<VulkanBuffer>& vkBuffers);

    //-Command buffer initialisation functions----------------------------//
    void createCommandPool(VkCommandPool* commandPool, VkCommandPoolCreateFlags flags);
    void createCommandBuffers(std::vector<VkCommandBuffer>* commandBuffers, VkCommandPool& commandPool);

    //-Record command buffers for rendering (geom and gui)----------------//
    void recordGeometryCommandBuffer(size_t cmdBufferIndex);
    void recordGuiCommandBuffer(size_t cmdBufferIndex);

    //-Window/Input Callbacks---------------------------------------------//
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    //-Sync structures----------------------------------------------------//
    void createSyncObjects();

    //-The main loop------------------------------------------------------//
    void mainLoop();

    //-Per frame functions------------------------------------------------//
    void drawFrame();
    void setGUI();
    int processKeyInput();
    void processMouseInput(glm::dvec2& offset);

    //-End of application cleanup-----------------------------------------//
    void cleanup();

private:
    //-Members------------------------------------------------------------//
    GLFWwindow* window;

    VulkanSetup     vkSetup; // instance, device (logical, physical), ...
    SwapChain   swapChain; // sc images, pipelines, ...
    FramebufferData framebufferData;
    GBuffer gBuffer;

    Model model;

    VulkanBuffer uniforms;
    VulkanBuffer vertexBuffer;
    VulkanBuffer indexBuffer;

    std::vector<Texture> textures;

    Camera camera;

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    std::vector<VkDescriptorSet> descriptorSets; // descriptor set handles

    VkCommandPool renderCommandPool;
    std::vector<VkCommandBuffer> renderCommandBuffers;

    VkCommandPool imGuiCommandPool;
    std::vector<VkCommandBuffer> imGuiCommandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores; // 1 semaphore per frame, GPU-GPU sync
    std::vector<VkSemaphore> renderFinishedSemaphores;

    std::vector<VkFence> inFlightFences; // 1 fence per frame, CPU-GPU sync
    std::vector<VkFence> imagesInFlight;

    glm::vec3 translate = glm::vec3(0.0f);
    glm::vec3 rotate = glm::vec3(0.0f);;
    float scale = 1.0f;
    
    bool shouldExit         = false;
    bool framebufferResized = false;
    bool firstMouse         = true;

    glm::dvec2 prevMouse;
    glm::dvec2 currMouse;

    std::chrono::steady_clock::time_point prevTime;
    float deltaTime;

    size_t currentFrame = 0;
    uint32_t imageIndex = 0; // idx of curr sc image
};

//
// Template function definitions
//

#endif // !APPLICATION_H