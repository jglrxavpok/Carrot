//
// Created by jglrxavpok on 20/02/2022.
//

#include "CameraComponent.h"
#include <core/utils/ImGuiUtils.hpp>

namespace Carrot::ECS {

    glm::mat4 CameraComponent::makeProjectionMatrix() const {
        if(isOrthographic) {
            glm::mat4 projectionMatrix = glm::ortho(-orthoSize.x / 2.0f, orthoSize.x / 2.0f, -orthoSize.y / 2.0f, orthoSize.y / 2.0f, 0.0f, orthoSize.z);
            projectionMatrix[1][1] *= -1; // convert to Vulkan coordinates (from OpenGL)
            return projectionMatrix;
        } else {
            glm::mat4 projectionMatrix = glm::perspective(perspectiveFov, PerspectiveAspectRatio, perspectiveNear, perspectiveFar);
            projectionMatrix[1][1] *= -1; // convert to Vulkan coordinates (from OpenGL)
            return projectionMatrix;
        }
    }
}