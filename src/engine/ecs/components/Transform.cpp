//
// Created by jglrxavpok on 26/02/2021.
//
#include "Transform.h"
#include <engine/ecs/World.h>

namespace Carrot::ECS {
    glm::mat4 Transform::toTransformMatrix() const {
        auto modelRotation = glm::toMat4(rotation);
        auto matrix = glm::translate(glm::mat4(1.0f), position) * modelRotation * glm::scale(glm::mat4(1.0f), scale);
        auto parent = getEntity().getParent();
        if(parent) {
            if(auto parentTransform = getEntity().getWorld().getComponent<Transform>(parent.value())) {
                return parentTransform->toTransformMatrix() * matrix;
            }
        }
        return matrix;
    }

    void Transform::drawInspectorInternals(const Render::Context& renderContext) {
        {
            float arr[] = { position.x, position.y, position.z };
            if (ImGui::DragFloat3("Position", arr)) {
                position = { arr[0], arr[1], arr[2] };
            }
        }

        {
            float arr[] = { scale.x, scale.y, scale.z };
            if (ImGui::DragFloat3("Scale", arr)) {
                scale = { arr[0], arr[1], arr[2] };
            }
        }

        {
            float arr[] = { rotation.x, rotation.y, rotation.z, rotation.w };
            if (ImGui::DragFloat4("Rotation", arr)) {
                rotation = { arr[3], arr[0], arr[1], arr[2] };
            }
        }
    }
}