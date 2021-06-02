//
// Definition of the loader class
//

#include <utils/Loader.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <iostream>

void Loader::loadGltfScene(const std::string& path) {
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;

	std::string err;
	std::string warn;

	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);

	if (!warn.empty()) {
		std::cout << warn;
	}

	if (!err.empty()) {
		throw std::runtime_error(err);
	}

	if (!ret) {
		throw std::runtime_error("Could not parse .gltf file");
	}

	for (size_t i = 0; i < model.bufferViews.size(); i++) {
		std::cout << model.bufferViews[i].name << '\n';
	}

	std::cout << "# scenes: " << model.scenes.size() << '\n';

	std::cout << "# meshes: " << model.meshes.size() << '\n';
}

void Loader::loadTextureImage(const std::string& path) {

}