///////////////////////////////////////////////////////
// Camera class declaration
///////////////////////////////////////////////////////

//
// Represents a camera in a scene. Updates orientation and position based on 
// keyboard input.
//

#ifndef CAMERA_H
#define CAMERA_H

#include <utils/Utils.h>

#include <common/Orientation.h>

#include <glm/glm.hpp> // vectors, matrices
#include <glm/gtc/quaternion.hpp> // the quaternions

// camera enums
enum class CameraMovement : unsigned char {
	PitchUp = 0x00,
	PitchDown = 0x10,
	RollLeft = 0x20,
	RollRight = 0x30,
	YawLeft = 0x40,
	YawRight = 0x50,
	Right = 0x60,
	Left = 0x70,
	Forward = 0x80,
	Backward = 0x90,
	Upward = 0xA0,
	Downward = 0xB0
};

class Camera {
public: 
	//-Camera constructor----------------------------------------------------------------------------------------//
	Camera(glm::vec3 initPos = glm::vec3(0.0f), float initAngleSpeed = 0.0f, float initPosSpeed = 0.0f) 
		: position(initPos), angleChangeSpeed(initAngleSpeed), positionChangeSpeed(initPosSpeed) {}

public:
	//-Usefult getters-------------------------------------------------------------------------------------------//
	glm::mat4 getViewMatrix();
	glm::mat4 getViewMatrix(const glm::vec3& pos);
	Orientation getOrientation();

	//-Input-----------------------------------------------------------------------------------------------------//
	void processInput(CameraMovement camMove, float deltaTime); // just keyborad input

private:
	//-Update orientation----------------------------------------------------------------------------------------//
	void updateCamera();

public:
	//-Members---------------------------------------------------------------------------------------------------//
	glm::vec3 position;

	Orientation orientation;

	float angleChangeSpeed;
	float positionChangeSpeed;
};

#endif // !CAMERA_H
