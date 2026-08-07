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

#include "ComponentReflection.h"

namespace Carrot::ECS {
    struct CameraComponent: public ReflectionComponent<CameraComponent> {
        using ReflectionComponent::ReflectionComponent;

        /// Is this the primary camera? Having multiple cameras with this flag will only apply the camera from the newest entity
        FIELD(bool, isPrimary, "Primary", false);
        FIELD(bool, isOrthographic, "Orthographic", false);

        // optional for backwards compatibility
        OPTIONAL_FIELD(glm::vec3, orthoSize, "Orthographic bounds", glm::vec3{ 1.0f });

        OPTIONAL_FIELD(float, perspectiveNear, "Perspective Near", 0.1f);
        OPTIONAL_FIELD(float, perspectiveFar, "Perspective Far", 1000.0f);
        OPTIONAL_FIELD(float, perspectiveFov, "Perspective FOV", glm::radians(70.0f));

        /// viewport in which to use this camera. Set to null for all viewports
        OPTIONAL_FIELD(Carrot::Identifier, targetViewportID, "Target Viewport", {});
        constexpr static float PerspectiveAspectRatio = 16.0f / 9.0f; // TODO: take viewport size into account

        glm::mat4 makeProjectionMatrix(const Carrot::Render::Viewport& viewport) const;

        static void applyRewriteRules(Carrot::DocumentElement& doc);
    };
}

ADD_COMPONENT_ID(Carrot::ECS, Camera)