///////////////////////////////////////////////////////
// Light class and derived Lights
///////////////////////////////////////////////////////

#ifndef SPOT_LIGHT_H
#define SPOT_LIGHT_H

#include <hpg/Image.h>
#include <common/types.h>	

#include <glm/gtc/matrix_transform.hpp>

class SpotLight {
public:
	SpotLight(glm::vec3 dir = { 0.0f, 0.0f, 0.0f }, F32 n = 0.0f, F32 f = 0.0f) : direction(dir), nearZ(n), farZ(f) {}

	glm::mat4 getMVP(glm::mat4 model = glm::mat4(1.0f)) {
		glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, nearZ, farZ);
		glm::mat4 view = glm::lookAt(direction, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
		proj[1][1] *= -1.0f;
		proj[0][0] *= -1.0f;
		return proj * view * model;
	}

	glm::mat4 getView() {
		return glm::lookAt(direction, { 0.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f });
	}

	glm::vec3 direction;

	F32 nearZ;
	F32 farZ;
};

#endif // !SPOT_LIGHT_H
