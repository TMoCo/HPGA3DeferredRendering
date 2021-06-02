///////////////////////////////////////////////////////
// Orientation class declaration
///////////////////////////////////////////////////////

//
// A class representing an orientation in 3D with some utility methods.
//

#ifndef ORIENTATION_H
#define ORIENTATION_H

#include <app/AppConstants.h>

#include <utils/Utils.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>


class Orientation {
public:
	//-Orientation constructor--------------------------------------------//
	Orientation() : front(WORLD_FRONT), up(WORLD_UP), right(WORLD_RIGHT) {}

public:
	//-Utility methods----------------------------------------------------//
	inline void applyRotation(const glm::quat& rotation) {
		front = glm::rotate(rotation, front);
		up    = glm::rotate(rotation, up);
		right = glm::cross(front, up);
	}

	inline void applyRotation(const glm::vec3& axis, float angle) {
		orientation *= glm::angleAxis(angle, axis * orientation);
		update();
	}

	inline void rotateToOrientation(const Orientation& targetOrientation) {
		// get the target orientation as a matrix
		glm::mat3 targetAsMatrix;
		targetAsMatrix[0] = targetOrientation.right; // x
		targetAsMatrix[1] = targetOrientation.up;    // y
		targetAsMatrix[2] = targetOrientation.front; // z

		// rotation to the other orientation is the inverse of the other orientation matrix (so transpose)
		glm::mat3 rotation = glm::transpose(targetAsMatrix);
		//
		//applyRotation(glm::qua)
		// rotate to new orientation
		front = glm::normalize(front * rotation);
		up	  = glm::normalize(up * rotation);
		right = glm::cross(front, up);
	}

	inline glm::mat4 toWorldSpaceRotation() {
		// return the orientation as a rotation in world space (apply q to the world axes
		glm::mat4 rotation(1.0f);
		rotation[0] = glm::rotate(orientation, glm::vec4(WORLD_RIGHT, 0.0f));
		rotation[1] = glm::rotate(orientation, glm::vec4(WORLD_UP, 0.0f));
		rotation[2] = glm::rotate(orientation, glm::vec4(WORLD_FRONT, 0.0f));
		return rotation;
	}

	inline glm::mat4 toModelSpaceRotation(const glm::mat4& model) {
		// return the orientation as a rotation in world space (apply q to the world axes
		glm::mat4 rotation(1.0f);
		rotation[0] = glm::rotate(orientation, model[0]);
		rotation[1] = glm::rotate(orientation, model[1]);
		rotation[2] = glm::rotate(orientation, model[2]);
		return rotation;
	}

private:
	inline void update() {
		front = glm::vec3(
			2.0f * orientation.x * orientation.z + 2.0f * orientation.y * orientation.w,
			2.0f * orientation.y * orientation.z - 2.0f * orientation.x * orientation.w,
			1.0f - 2.0f * (orientation.x * orientation.x) - 2.0f * (orientation.y * orientation.y));
		up = glm::vec3(
			-(2.0f * orientation.x * orientation.y + 2.0f * orientation.z * orientation.w),
			-(1.0f - 2.0f * (orientation.x * orientation.x) - 2.0f * (orientation.z * orientation.z)),
			-(2.0f * orientation.y * orientation.z - 2.0f * orientation.x * orientation.w));
		right = glm::vec3(
			1.0f - 2.0f * (orientation.y * orientation.y) - 2.0f * (orientation.z * orientation.z),
			2.0f * orientation.x * orientation.y + 2.0f * orientation.z * orientation.w,
			2.0f * orientation.x * orientation.z - 2.0f * orientation.y * orientation.w);
	}

public:	
	//-Members-------------------------------------------------//
	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;

	glm::quat orientation;
};


#endif // !ORIENTATION_H