//
// A class representing a cube
//

#ifndef CUBE_H
#define CUBE_H

#include <common/types.h>
#include <glm/glm.hpp>

class Cube {
public:
	Cube(F32 x = 1.0f, F32 y = 1.0f, F32 z = 1.0f) : extentX(x), extentY(y), extentZ(z) {}
	// data describing a plane
	F32 extentX;
	F32 extentY;
	F32 extentZ;
};

#endif // !CUBE_H