//
// Camera class definition
//

#include <utils/Assert.h>

#include <common/Camera.h> // the camera class

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

glm::mat4 Camera::getViewMatrix() {
    return glm::lookAt(position, position + orientation.front, orientation.up); // look at in front of the camera
}

glm::mat4 Camera::getViewMatrix(const glm::vec3& pos) {
    return glm::lookAt(position, glm::normalize(pos - position), orientation.up); // look at a certain point
}

Orientation Camera::getOrientation() {
    return orientation;
}

void Camera::processInput(CameraMovement camMove, float deltaTime) {
    pitch = roll = yaw = 0.0f; // reset pitch yaw and roll
    if (camMove == CameraMovement::PitchUp)
        pitch = angleChangeSpeed * deltaTime;
    if (camMove == CameraMovement::PitchDown)
        pitch = -angleChangeSpeed * deltaTime;
    if (camMove == CameraMovement::RollRight)
        roll = angleChangeSpeed * deltaTime;
    if (camMove == CameraMovement::RollLeft)
        roll = -angleChangeSpeed * deltaTime;
    if (camMove == CameraMovement::YawLeft)
        yaw = angleChangeSpeed * deltaTime;
    if (camMove == CameraMovement::YawRight)
        yaw = -angleChangeSpeed * deltaTime;
    if (camMove == CameraMovement::Left)
        position -= orientation.right * deltaTime * positionChangeSpeed;
    if (camMove == CameraMovement::Right)
        position += orientation.right * deltaTime * positionChangeSpeed;
    if (camMove == CameraMovement::Forward)
        position += orientation.front * deltaTime * positionChangeSpeed;
    if (camMove == CameraMovement::Backward)
        position -= orientation.front * deltaTime * positionChangeSpeed;
    // update the camera accordingly
    updateCamera();
}

void Camera::updateCamera() {
    // we need to update the camera's axes: get the rotation from the camera input
    glm::quat rotation = glm::angleAxis(glm::radians(yaw), orientation.up) * 
                         glm::angleAxis(glm::radians(pitch), orientation.right) *
                         glm::angleAxis(glm::radians(roll), orientation.front);
    // apply the rotation to the current orientation
    orientation.applyRotation(rotation);
}

void Camera::setDirection(const glm::vec3& dir) {
    float length = glm::length(dir);
    m_assert((length > 0.99f) && (length < 1.01f), "Invalid direction vector (must be unit)!");
    orientation.front = dir;
    orientation.right = glm::cross(dir, WORLD_UP);
    orientation.up = glm::cross(dir, orientation.right);
}