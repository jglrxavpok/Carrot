//
// Created by jglrxavpok on 06/12/2020.
//

#include "Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

Carrot::Camera::Camera(float fov, float aspectRatio, float zNear, float zFar, glm::vec3 up): up(up), position(), target() {
    projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, zNear, zFar);
    projectionMatrix[1][1] *= -1; // convert to Vulkan coordinates (from OpenGL)
}

const glm::mat4& Carrot::Camera::getProjectionMatrix() const {
    return projectionMatrix;
}

const glm::vec3& Carrot::Camera::getUp() const {
    return up;
}