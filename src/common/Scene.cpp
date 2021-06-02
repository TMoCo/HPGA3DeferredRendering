//
// Definition of the Scene class
//

#include <common/Scene.h>

Scene::Scene(const std::string& path) {
	// constructor with path argument calls the load scene method automatically
	LoadScene(path);
}

void Scene::LoadScene(const std::string& path) {
	std::string error;
	bool r = loader.LoadASCIIFromFile(&scene, &error, path);
	if (!error.empty()) {
		std::cout << error << std::endl;
	}
	if (!r) {
		throw std::runtime_error("failed to parse gltf scene!");
	}
}