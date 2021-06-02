//
// A master loader class for loading a select few types of data:
// gltf scenes
// textures from images (all those supported by stbi)
//

#ifndef LOADER_H
#define LOADER_H

#include <string>

class Loader {
public:
	Loader() {}

	void loadGltfScene(const std::string& path);

	void loadTextureImage(const std::string& path);
};

#endif // !LOADER_H
