//
// Created by jglrxavpok on 20/02/2022.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <core/utils/JSON.h>
#include "engine/render/Viewport.h"
#include <imgui.h>

namespace Carrot::ECS {
    struct CameraComponent: public IdentifiableComponent<CameraComponent> {
        /// Is this the primary camera? Having multiple cameras with this flag will only apply the camera from the newest entity
        bool isPrimary = false;
        bool isOrthographic = false;
        glm::vec3 orthoSize { 1.0f };

        float perspectiveNear = 0.1f;
        float perspectiveFar = 1000.0f;
        float perspectiveFov = glm::radians(70.0f);
        constexpr static float PerspectiveAspectRatio = 16.0f / 9.0f; // TODO: take viewport size into account

        explicit CameraComponent(Entity entity): IdentifiableComponent<CameraComponent>(std::move(entity)) {};

        explicit CameraComponent(const Carrot::DocumentElement& json, Entity entity);

        Carrot::DocumentElement serialise() const override;

        const char *const getName() const override {
            return "Camera";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<CameraComponent>(newOwner);
            result->isPrimary = isPrimary;
            result->isOrthographic = isOrthographic;
            result->orthoSize = orthoSize;
            result->perspectiveNear = perspectiveNear;
            result->perspectiveFar = perspectiveFar;
            result->perspectiveFov = perspectiveFov;
            return result;
        }

        glm::mat4 makeProjectionMatrix(const Carrot::Render::Viewport& viewport) const;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::CameraComponent>::getStringRepresentation() {
    return "Camera";
}
