//
// Camera class definition
//

#include <utils/Assert.h>
#include <utils/Print.h>

#include <common/Camera.h> // the camera class

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

glm::mat4 Camera::getViewMatrix() {
    // compute rotation based on orientation and translation based on position
    glm::mat4 rotate = glm::mat4_cast(orientation.orientation);
    return rotate * glm::translate(glm::mat4(1.0f), -position);
}

Orientation Camera::getOrientation() {
    return orientation;
}

void Camera::processInput(CameraMovement camMove, float deltaTime) {
    switch (camMove) {
    case CameraMovement::PitchUp:
        orientation.applyRotation(Axes::X, -angleChangeSpeed * deltaTime);
        break;
    case CameraMovement::PitchDown:
        orientation.applyRotation(Axes::X, angleChangeSpeed * deltaTime);
        break;
    case CameraMovement::RollRight:
        orientation.applyRotation(Axes::Z, angleChangeSpeed * deltaTime);
        break;
    case CameraMovement::RollLeft:
        orientation.applyRotation(Axes::Z, -angleChangeSpeed * deltaTime);
        break;
    case CameraMovement::YawLeft:
        orientation.applyRotation(Axes::Y, -angleChangeSpeed * deltaTime);
        break;
    case CameraMovement::YawRight:
        orientation.applyRotation(Axes::Y, angleChangeSpeed * deltaTime);
        break;
    case CameraMovement::Left:
        position += glm::rotate(orientation.orientation, Axes::LEFT) * deltaTime * positionChangeSpeed;
        break;
    case CameraMovement::Right: 
        position += glm::rotate(orientation.orientation, Axes::RIGHT) * deltaTime * positionChangeSpeed;
        break;
    case CameraMovement::Forward:
        position += glm::rotate(orientation.orientation, Axes::FRONT) * deltaTime * positionChangeSpeed;
        break;
    case CameraMovement::Backward:
        position += glm::rotate(orientation.orientation, Axes::BACK) * deltaTime * positionChangeSpeed;
        break;
    case CameraMovement::Upward:
        position += glm::rotate(orientation.orientation, Axes::UP) * deltaTime * positionChangeSpeed;
        break;
    case CameraMovement::Downward:
        position += glm::rotate(orientation.orientation, Axes::DOWN) * deltaTime * positionChangeSpeed;
        break;
    }
}