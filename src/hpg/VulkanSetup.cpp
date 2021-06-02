///////////////////////////////////////////////////////
//
// Definition of the VulkanSetup class
//
///////////////////////////////////////////////////////

// app constants like app name
#include <app/AppConstants.h>

// include the class declaration
#include <hpg/VulkanSetup.h>

// glfw window library
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// reporting and propagating exceptions
#include <iostream> 
#include <stdexcept>

#include <set>
#include <string>

//
// INITIALISATION AND DESTRUCTION
//

void VulkanSetup::initSetup(GLFWwindow* theWindow) {
    // keep a reference to the glfw window for now
    window = theWindow;
    // start by creating a vulkan instance
    createInstance();

    // setup the debug messenger with the validation layers
    setupDebugMessenger();

    // create the surface to draw to
    createSurface();

    // pick the physical device we want to use, making sure it is appropriate
    pickPhysicalDevice();

    // create the logical device for interfacing with the physical device
    createLogicalDevice();

    // we got this far so signal that the setup was complete
    setupComplete = true;
}

void VulkanSetup::cleanupSetup() {
    // remove the logical device, no direct interaction with instance so not passed as argument
    vkDestroyDevice(device, nullptr);
    // destroy the window surface
    vkDestroySurfaceKHR(instance, surface, nullptr);
    // if debug activated, remove the messenger
    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }
    // only called before program exits, destroys the vulkan instance
    vkDestroyInstance(instance, nullptr);
}

//
// INSTANCE
//

void VulkanSetup::createInstance() {
    // if we have enabled validation layers and some requested layers aren't available, throw error
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    // tells the driver how to optimise for our purpose
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = APP_NAME.data();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = ENGINE_NAME.data();
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0; // version of API used

    // create a VkInstanceCreateInfo struct, not optional!
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; // type of instance we are creating
    createInfo.pApplicationInfo = &appInfo; // pointer to 

    auto extensions = getRequiredExtensions();
    // update createInfo
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // create a debug messenger before the instance is created to capture any errors in creation process
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    // include valdation layers if enables
    if (enableValidationLayers) {
        // save layer count, cast size_t to uin32_t
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    // we can now create the instance (pointer to struct, pointer to custom allocator callbacks, 
    // pointer to handle that stores the new object)
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) { // check everything went well by comparing returned value
        throw std::runtime_error("failed to create a vulkan instance!");
    }
}

std::vector<const char*> VulkanSetup::getRequiredExtensions() {
    // start by getting the glfw extensions, nescessary for displaying something in a window.
    // platform agnostic, so need an extension to interface with window system. Use GLFW to return
    // the extensions needed for platform and passed to createInfo struct
    uint32_t glfwExtensionCount = 0; // initialise extension count to 0, changed later
    const char** glfwExtensions; // array of strings with extension names
    // get extension count
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // glfwExtensions is an array of strings, we give the vector constructor a range of values from glfwExtensions to 
    // copy (first value at glfwExtensions, a pointer, to last value, pointer to first + nb of extensions)
    // a vector containing the values from glfwExtensions. 
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // add the VK_EXT_debug_utils with macro on condition debug is activated
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // return the vector
    return extensions;
}

//
// VALIDATION
//

void VulkanSetup::setupDebugMessenger() {
    // do nothing if we are not in debug mode
    if (!enableValidationLayers) return;

    // create messenger info
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    // set create info's parameters
    populateDebugMessengerCreateInfo(createInfo);

    // create the debug messenger
    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

// proxy function handles finding the extension function vkCreateDebugUtilsMessengerEXT 
VkResult VulkanSetup::CreateDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    // vkGetInstanceProcAddr returns nullptr if the debug messenger creator function couldn't be loaded, otherwise a pointer to the function
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != nullptr) {
        // return the result of the function
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

// similar to above function but for destroying a debug messenger
void VulkanSetup::DestroyDebugUtilsMessengerEXT(VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanSetup::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) { // some user data
    // message severity flags, values can be used to check how message compares to a certain level of severity
    // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: Diagnostic message
    // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT : Informational message like the creation of a resource
    // VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT : Message about behavior that is not necessarily an error, but very likely a bug in your application
    // VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT : Message about behavior that is invalid and may cause crashes
    // message type flags
    // VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: Some event has happened that is unrelated to the specification or performance
    // VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT : Something has happened that violates the specification or indicates a possible mistake
    // VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT : Potential non - optimal use of Vulkan
    // refers to a struct with the details of the message itself
    // pMessage : The debug message as a null - terminated string
    // pObjects : Array of Vulkan object handles related to the message
    // objectCount : Number of objects in array
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void VulkanSetup::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    // the creation of a messenger create info is put in a separate function for use to debug the creation and destruction of 
    // a VkInstance object
    createInfo = {};
    createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    // types of callbacks to be called for
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    if (enableVerboseValidation) {
        createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    }
    // filter which message type filtered by callback
    createInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    // pointer to call back function
    createInfo.pfnUserCallback = debugCallback;
}

bool VulkanSetup::checkValidationLayerSupport() {
    uint32_t layerCount;
    // get the layer count
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    // create a vector of layers of size layercount
    std::vector<VkLayerProperties> availableLayers(layerCount);
    // get all available layers and store in the vector
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    // iterate over the desired validation layers
    for (const char* layerName : validationLayers) {
        bool layerFound = false;
        // iterate over the available layers (We could have used a set as is the case for device extensions check)
        for (const auto& layerProperties : availableLayers) {
            // check that the validation layers exist in available layers
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        // if not found, then we can't use that layer
        if (!layerFound) {
            return false;
        }
    }

    return true;
}

//
// SURFACE
//

void VulkanSetup::createSurface() {
    // takes simple arguments instead of structs
    // object is platform agnostic but creation is not, this is handled by the glfw method
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

//
// DEVICE
// 

void VulkanSetup::pickPhysicalDevice() {
    // similar to extensions, gets the physical devices available
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    // handles no devices
    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    // get a vector of physical devices
    std::vector<VkPhysicalDevice> devices(deviceCount);
    // store all physical devices in the vector
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // iterate over available physical devices and check that they are suitable
    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            break;
        }
    }

    // if the physicalDevice handle is still null, then no suitable devices were found
    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

bool VulkanSetup::isDeviceSuitable(VkPhysicalDevice device) {
    // if we wanted to look at the device properties and features in more detail:
    // VkPhysicalDeviceProperties deviceProperties;
    // vkGetPhysicalDeviceProperties(device, &deviceProperties);
    // VkPhysicalDeviceFeatures deviceFeatures;
    // vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    // we could determine which device is the most suitable by giving each a score
    // based on the contents of the structs

    // get the queues
    utils::QueueFamilyIndices indices = utils::QueueFamilyIndices::findQueueFamilies(device, surface);

    bool extensionsSupported = checkDeviceExtensionSupport(device);
    // NB the availability of a presentation queue implies that swap chain extension is supported, but best to be explicit about this

    bool swapChainAdequate = false;
    if (extensionsSupported) { // if extension supported, in our case extension for the swap chain
        // find out more about the swap chain details
        VulkanSetup::SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        // at least one supported image format and presentation mode is sufficient for now
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    // get the device's supported features
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    // return the queue family index (true if a value was initialised), device supports extension and swap chain is adequate (phew)
    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool VulkanSetup::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    // intit the extension count
    uint32_t extensionCount;
    // set the extension count using the right vulkan enumerate function 
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    // a vector container for the properties of the available extensions
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    // same function used to get extension count, but add the pointer to vector data to store the properties struct
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    // wrap the const vector of extensions deviceExtensions defined at top of header file into a set, to get unique extensions names
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    // loop through available extensions, erasing any occurence of the required extension(s)
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    // if the required extensions vector is empty, then they were erased because they are available, so return true with empty() 
    return requiredExtensions.empty();
}

VulkanSetup::SwapChainSupportDetails VulkanSetup::querySwapChainSupport(VkPhysicalDevice device) {
    VulkanSetup::SwapChainSupportDetails details;
    // query the surface capabilities and store in a VkSurfaceCapabilities struct
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities); // takes into account device and surface when determining capabilities

    // same as we have seen many times before
    uint32_t formatCount;
    // query the available formats, pass null ptr to just set the count
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    // if there are formats
    if (formatCount != 0) {
        // then resize the vector accordingly
        details.formats.resize(formatCount);
        // and set details struct fromats vector with the data pointer
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    // exact same thing as format for presentation modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void VulkanSetup::createLogicalDevice() {
    // query the queue families available on the device
    utils::QueueFamilyIndices indices = utils::QueueFamilyIndices::findQueueFamilies(physicalDevice, surface);

    // create a vector containing VkDeviceQueueCreqteInfo structs
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    // using a set makes sure that there are no dulpicate references to a same queue!
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    // queue priority, for now give queues the same priority
    float queuePriority = 1.0f;
    // loop over the queue families in the set
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        // specify which queue we want to create, initialise struct at 0
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; // information in the struct
        queueCreateInfo.queueFamilyIndex = queueFamily; // the family index
        queueCreateInfo.queueCount       = 1; // number of queues
        queueCreateInfo.pQueuePriorities = &queuePriority; // between 0 and 1, influences sheduling of queue commands 
        // push the info on the vector
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // queries support certain features (like geometry shaders, other things in the vulkan pipeline...)
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE; // we want the device to use anisotropic filtering if available

    // the struct containing the device info
    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; // inform on type of struct
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size()); // the number of queues
    createInfo.pQueueCreateInfos       = queueCreateInfos.data(); // pointer to queue(s) info, here the raw underlying array in a vector (guaranteed contiguous!)

    createInfo.pEnabledFeatures        = &deviceFeatures; // desired device features
    // setting validation layers and extensions is per device
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()); // the number of desired extensions
    createInfo.ppEnabledExtensionNames = deviceExtensions.data(); // pointer to the vector containing the desired extensions 

    // older implementation compatibility, no disitinction instance and device specific validations
    if (enableValidationLayers) {
        // these fields are ignored by newer vulkan implementations
        createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    // instantiate a logical device from the create info we've determined
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    // set the graphics queue handle, only want a single queue so use index 0
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    // set the presentation queue handle like above
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}
