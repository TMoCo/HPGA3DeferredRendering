//
// A convenience header file for application constants like app name
//

#ifndef APP_CONSTANTS_H
#define APP_CONSTANTS_H

#include <stdint.h> // uint32_t
#include <vector> // vector container
#include <string> // string class
#include <optional> // optional wrapper
#include <ios> // streamsize

#include <glm/glm.hpp>

//
// App constants
//

// constants for window dimensions
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// strings for the vulkan instance
const std::string APP_NAME    = "Deferred Rendering";
const std::string ENGINE_NAME = "No Engine";

// max size for reading a line 
const std::streamsize MAX_SIZE = 1048;

// world axes
const glm::vec3 WORLD_RIGHT = glm::vec3(-1.0f, 0.0f, 0.0f);
const glm::vec3 WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);
const glm::vec3 WORLD_FRONT = glm::vec3(0.0f, 0.0f, -1.0f);

// paths to the model
const std::string MODEL_PATH = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\Assets\\SuzanneGltf\\Suzanne.gltf";

// vertex shaders
const std::string VERT_SHADER = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\DeferredRendering\\src\\shaders\\offscreen.vert.spv";
const std::string FRAG_SHADER = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\DeferredRendering\\src\\shaders\\offscreen.frag.spv";

#endif // !APP_CONSTANTS_H