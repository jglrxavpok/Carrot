//
// Created by jglrxavpok on 20/02/2022.
//

#include "CameraComponent.h"
#include <core/utils/ImGuiUtils.hpp>

namespace Carrot::ECS {
    void CameraComponent::drawInspectorInternals(const Render::Context& renderContext, bool& modified) {
        ImGui::Checkbox("Primary", &isPrimary);
        ImGui::BeginGroup();
        if(ImGui::RadioButton("Perspective", !isOrthographic)) {
            isOrthographic = false;
        }
        ImGui::SameLine();
        if(ImGui::RadioButton("Orthographic", isOrthographic)) {
            isOrthographic = true;
        }
        ImGui::EndGroup();

        if(isOrthographic) {
            float arr[3] = { orthoSize.x, orthoSize.y, orthoSize.z };
            if(ImGui::DragFloat3("Bounds", arr, 0.5f)) {
                orthoSize = { arr[0], arr[1], arr[2] };
            }
        } else {
            ImGui::DragFloatRange2("Z Range", &perspectiveNear, &perspectiveFar, 0.0f, 10000.0f);
            ImGui::SliderAngle("FOV", &perspectiveFov, 0.001f, 360.0f);
        }
    }

    glm::mat4 CameraComponent::makeProjectionMatrix() const {
        if(isOrthographic) {
            return glm::ortho(-orthoSize.x / 2.0f, orthoSize.x / 2.0f, -orthoSize.y / 2.0f, orthoSize.y / 2.0f, 0.0f, orthoSize.z);
        } else {
            glm::mat4 projectionMatrix = glm::perspective(perspectiveFov, PerspectiveAspectRatio, perspectiveNear, perspectiveFar);
            projectionMatrix[1][1] *= -1; // convert to Vulkan coordinates (from OpenGL)
            return projectionMatrix;
        }
    }
}