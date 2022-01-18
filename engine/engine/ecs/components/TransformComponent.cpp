//
// Created by jglrxavpok on 26/02/2021.
//
#include "TransformComponent.h"
#include <engine/ecs/World.h>
#include <core/utils/JSON.h>
#include <glm/gtx/matrix_decompose.hpp>

namespace Carrot::ECS {
    glm::mat4 TransformComponent::toTransformMatrix() const {
        auto parent = getEntity().getParent();
        if(parent) {
            if(auto parentTransform = getEntity().getWorld().getComponent<TransformComponent>(parent.value())) {
                return localTransform.toTransformMatrix(&parentTransform->localTransform);
            }
        }
        return localTransform.toTransformMatrix();
    }

    void TransformComponent::drawInspectorInternals(const Render::Context& renderContext, bool& modified) {
        {
            float arr[] = { localTransform.position.x, localTransform.position.y, localTransform.position.z };
            if (ImGui::DragFloat3("Position", arr)) {
                localTransform.position = { arr[0], arr[1], arr[2] };
                modified = true;
            }
        }

        {
            float arr[] = { localTransform.scale.x, localTransform.scale.y, localTransform.scale.z };
            if (ImGui::DragFloat3("Scale", arr)) {
                localTransform.scale = { arr[0], arr[1], arr[2] };
                modified = true;
            }
        }

        {
            float arr[] = { localTransform.rotation.x, localTransform.rotation.y, localTransform.rotation.z, localTransform.rotation.w };
            if (ImGui::DragFloat4("Rotation", arr)) {
                localTransform.rotation = { arr[3], arr[0], arr[1], arr[2] };
                modified = true;
            }
        }
    }

    TransformComponent::TransformComponent(const rapidjson::Value& json, Entity entity): TransformComponent(std::move(entity)) {
        localTransform.loadJSON(json);
    };

    rapidjson::Value TransformComponent::toJSON(rapidjson::Document& doc) const {
        return localTransform.toJSON(doc.GetAllocator());
    }

    void TransformComponent::setGlobalTransform(const Carrot::Math::Transform& newTransform) {
        auto parent = getEntity().getParent();
        if(parent) {
            if (auto parentTransform = getEntity().getWorld().getComponent<TransformComponent>(parent.value())) {
                glm::mat4 inverseParent = glm::inverse(parentTransform->toTransformMatrix());
                glm::vec4 localTranslationH = inverseParent * glm::vec4{ newTransform.position, 1.0 };
                glm::vec3 localTranslation = { localTranslationH.x, localTranslationH.y, localTranslationH.z };
                glm::quat localRotation = glm::toQuat(inverseParent) * newTransform.rotation;

                glm::vec3 parentInverseScale;
                glm::quat parentRotation;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(inverseParent, parentInverseScale, parentRotation, translation, skew,perspective);

                localTransform.position = localTranslation;
                localTransform.scale = parentInverseScale * newTransform.scale;
                localTransform.rotation = localRotation * newTransform.rotation;
                return;
            }
        }

        localTransform = newTransform;
    }

    glm::vec3 TransformComponent::computeFinalPosition() const {
        glm::vec4 wpos {0, 0, 0, 1.0};
        wpos = toTransformMatrix() * wpos;
        return { wpos.x, wpos.y, wpos.z };
    }

    Carrot::Math::Transform TransformComponent::computeGlobalTransform() const {
        auto parent = getEntity().getParent();
        if(parent) {
            if (auto parentTransform = getEntity().getWorld().getComponent<TransformComponent>(parent.value())) {
                glm::mat4 finalTransform = toTransformMatrix();
                glm::vec3 globalScale;
                glm::quat globalRotation;
                glm::vec3 globalTranslation;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(finalTransform, globalScale, globalRotation, globalTranslation, skew,perspective);

                Carrot::Math::Transform computedGlobalTransform;
                computedGlobalTransform.position = globalTranslation;
                computedGlobalTransform.scale = globalScale;
                computedGlobalTransform.rotation = globalRotation;
                return computedGlobalTransform;
            }
        }

        return localTransform;
    }
}