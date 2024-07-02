//
// Created by jglrxavpok on 20/02/2022.
//

#include "CameraComponent.h"
#include <core/utils/ImGuiUtils.hpp>

namespace Carrot::ECS {

    glm::mat4 CameraComponent::makeProjectionMatrix(const Carrot::Render::Viewport& viewport) const {
        if(isOrthographic) {
            glm::mat4 projectionMatrix = glm::ortho(-orthoSize.x / 2.0f, orthoSize.x / 2.0f, -orthoSize.y / 2.0f, orthoSize.y / 2.0f, 0.0f, orthoSize.z);
            projectionMatrix[1][1] *= -1; // convert to Vulkan coordinates (from OpenGL)
            return projectionMatrix;
        } else {
            float aspectRatio = viewport.getSizef().x / viewport.getSizef().y;
            glm::mat4 projectionMatrix = glm::perspective(perspectiveFov, aspectRatio, perspectiveNear, perspectiveFar);
            projectionMatrix[1][1] *= -1; // convert to Vulkan coordinates (from OpenGL)
            return projectionMatrix;
        }
    }
}