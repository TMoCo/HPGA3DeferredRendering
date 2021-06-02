//
// A class that represents a scene loaded from a gltf file
//

#ifndef SCENE_H
#define SCENE_H

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif // !STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_LOADER_IMPLEMENTATION
#include <tiny_gltf_loader.h>

class Scene {
public:
	Scene() {}; // default constructor here
	Scene(const std::string& scenePath); // constructor from specified path

	// loads a scene at the specified path and returns the new scene
	void LoadScene(const std::string& path);

	// the scene in the gltf file
	tinygltf::Scene scene;

	// gltf loader
	tinygltf::TinyGLTFLoader loader;
};

#endif
