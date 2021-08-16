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

// paths to the model
const std::string MODEL_PATH = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\Assets\\SuzanneGltf\\Suzanne.gltf";

// path to the skybox
const std::string SKYBOX_PATH = "C:\\Users\\Tommy\\Documents\\Graphics\\CubeMaps\\sky\\";

// forward rendering shader paths
const std::string FWD_VERT_SHADER = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\DeferredRendering\\src\\shaders\\forward.vert.spv";
const std::string FWD_FRAG_SHADER = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\DeferredRendering\\src\\shaders\\forward.frag.spv";

// deferred rendering shader paths (offscreen, composition, skybox)
const std::string OFF_VERT_SHADER = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\DeferredRendering\\src\\shaders\\offscreen.vert.spv";
const std::string OFF_FRAG_SHADER = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\DeferredRendering\\src\\shaders\\offscreen.frag.spv";

const std::string COMP_VERT_SHADER = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\DeferredRendering\\src\\shaders\\composition.vert.spv";
const std::string COMP_FRAG_SHADER = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\DeferredRendering\\src\\shaders\\composition.frag.spv";

const std::string SKY_VERT_SHADER = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\DeferredRendering\\src\\shaders\\skybox.vert.spv";
const std::string SKY_FRAG_SHADER = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\DeferredRendering\\src\\shaders\\skybox.frag.spv";

const std::string SHADOWMAP_VERT_SHADER = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\DeferredRendering\\src\\shaders\\shadowmap.vert.spv";
const std::string SHADOWMAP_FRAG_SHADER = "C:\\Users\\Tommy\\Documents\\COMP4\\5822HighPerformanceGraphics\\A3\\DeferredRendering\\src\\shaders\\shadowmap.frag.spv";

namespace Axes {
	// world axes
	const glm::vec3 WORLD_RIGHT = glm::vec3(-1.0f, 0.0f, 0.0f);
	const glm::vec3 WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);
	const glm::vec3 WORLD_FRONT = glm::vec3(0.0f, 0.0f, -1.0f);

	const glm::vec3 X = glm::vec3(1.0f, 0.0f, 0.0f);
	const glm::vec3 Y = glm::vec3(0.0f, 1.0f, 0.0f);
	const glm::vec3 Z = glm::vec3(0.0f, 0.0f, 1.0f);

	// default directions
	const glm::vec3 RIGHT = glm::vec3(1.0f, 0.0f, 0.0f);
	const glm::vec3 LEFT = glm::vec3(-1.0f, 0.0f, 0.0f);
	const glm::vec3 UP = glm::vec3(0.0f, 1.0f, 0.0f);
	const glm::vec3 DOWN = glm::vec3(0.0f, -1.0f, 0.0f);
	const glm::vec3 FRONT = glm::vec3(0.0f, 0.0f, -1.0f);
	const glm::vec3 BACK = glm::vec3(0.0f, 0.0f, 1.0f);
}

#endif // !APP_CONSTANTS_H