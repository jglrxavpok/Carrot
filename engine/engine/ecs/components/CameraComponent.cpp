//
// Created by jglrxavpok on 20/02/2022.
//

#include "CameraComponent.h"

#include <core/io/DocumentHelpers.h>
#include <core/utils/ImGuiUtils.hpp>

namespace Carrot::ECS {

    CameraComponent::CameraComponent(const Carrot::DocumentElement& doc, Entity entity): CameraComponent(std::move(entity)) {
        isPrimary = doc["primary"].getAsBool();
        isOrthographic = doc["orthographic"].getAsBool();

        if(isOrthographic) {
            orthoSize = DocumentHelpers::read<3, float>(doc["ortho_bounds"]);
        } else {
            perspectiveNear = doc["z_near"].getAsDouble();
            perspectiveFar = doc["z_far"].getAsDouble();
            perspectiveFov = doc["fov"].getAsDouble();
        }
    }

    Carrot::DocumentElement CameraComponent::serialise() const {
        Carrot::DocumentElement obj;

        obj["primary"] = isPrimary;
        obj["orthographic"] = isOrthographic;
        if(isOrthographic) {
            obj["ortho_bounds"] = DocumentHelpers::write(orthoSize);
        } else {
            obj["z_near"] = perspectiveNear;
            obj["z_far"] = perspectiveFar;
            obj["fov"] = perspectiveFov;
        }

        return obj;
    }


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