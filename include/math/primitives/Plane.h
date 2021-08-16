//
// A class representing a plane
//

#ifndef PLANE_H
#define PLANE_H

#include <common/types.h>
#include <glm/glm.hpp>

class Plane {
public:
	Plane(F32 x = 1.0f, F32 y = 1.0f) : extentX(x), extentY(y) {}

	std::vector<Model::Vertex> getVertices() {
		return {
			{ {-extentX, -2.0f, extentY}, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
			{ {extentX, -2.0f, extentY}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },
			{ {extentX, -2.0f, -extentY}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },
			{ {-extentX, -2.0f, -extentY}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f} }
		};
	}

	std::vector<UI32> getIndices() {
		return { 0, 1, 2, 2, 3, 0 }; 
	}

	// data describing a plane
	F32 extentX;
	F32 extentY;
	std::vector<Model::Vertex> vertices;
	std::vector<UI32> indices;
};

#endif