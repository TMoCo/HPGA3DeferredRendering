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
	Orientation() : orientation(0.0f, 0.0f, 0.0f, 1.0f) {}

public:
	//-Utility methods----------------------------------------------------//
	inline void applyRotation(const glm::quat& rotation) {
		orientation = glm::normalize(rotation * orientation); // rotate orientarion quaternion by rotation quaternion
	}

	inline void applyRotation(const glm::vec3& axis, float angle) {
		orientation = glm::normalize(glm::angleAxis(angle, axis) * orientation);
	}

	inline void applyRotations(const glm::vec3& angles) {
		orientation *= glm::angleAxis(angles.x, Axes::X * orientation);
		orientation *= glm::angleAxis(angles.y, Axes::Y * orientation);
		orientation *= glm::angleAxis(angles.z, Axes::Z * orientation);
	}

	inline void rotateToOrientation(const Orientation& target) {
		orientation = glm::inverse(target.orientation) * orientation;
	}

	inline glm::mat4 toWorldSpaceRotation() {
		// return the orientation as a rotation in world space (apply q to the world axes
		glm::mat4 rotation(1.0f);
		rotation[0] = glm::rotate(orientation, glm::vec4(Axes::WORLD_RIGHT, 0.0f));
		rotation[1] = glm::rotate(orientation, glm::vec4(Axes::WORLD_UP, 0.0f));
		rotation[2] = glm::rotate(orientation, glm::vec4(Axes::WORLD_FRONT, 0.0f));
		return rotation;
	}

	inline glm::mat4 toModelSpaceRotation(const glm::mat4& model) {
		// return the orientation as a rotation in world space (apply q to the world axes)
		glm::mat4 rotation(1.0f);
		rotation[0] = glm::rotate(orientation, model[0]);
		rotation[1] = glm::rotate(orientation, model[1]);
		rotation[2] = glm::rotate(orientation, model[2]);
		return rotation;
	}

	inline glm::quat getOrientationQuaternion() {
		return orientation;
	}

private:
	inline void update() {}

public:	
	//-Members-------------------------------------------------//
	glm::quat orientation;
};


#endif // !ORIENTATION_H