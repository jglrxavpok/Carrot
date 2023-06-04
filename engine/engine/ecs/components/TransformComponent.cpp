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
                glm::mat4 parentGlobalTransform = parentTransform->toTransformMatrix();
                return parentGlobalTransform * localTransform.toTransformMatrix();
            }
        }
        return localTransform.toTransformMatrix();
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
                glm::quat parentRotation = glm::toQuat(inverseParent);
                glm::quat localRotation = parentRotation * newTransform.rotation;

                glm::vec3 parentInverseScale;
                glm::quat parentRotationFromMatrix;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(inverseParent, parentInverseScale, parentRotationFromMatrix, translation, skew,perspective);

                localTransform.position = localTranslation;
                //localTransform.scale = parentInverseScale * newTransform.scale;
                localTransform.rotation = localRotation;
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

    glm::vec3 TransformComponent::computeFinalScale() const {
        auto parent = getEntity().getParent();
        if(parent) {
            if (auto parentTransform = getEntity().getWorld().getComponent<TransformComponent>(parent.value())) {
                return parentTransform->computeFinalScale() * localTransform.scale;
            }
        }
        return localTransform.scale;
    }

    glm::quat TransformComponent::computeFinalOrientation() const {
        auto parent = getEntity().getParent();
        if(parent) {
            if (auto parentTransform = getEntity().getWorld().getComponent<TransformComponent>(parent.value())) {
                return parentTransform->computeFinalOrientation() * localTransform.rotation;
            }
        }
        return localTransform.rotation;
    }

    glm::vec3 TransformComponent::computeGlobalForward() const {
        constexpr glm::vec3 forward = glm::vec3{ 0.0f, 1.0f, 0.0f };
        return computeFinalOrientation() * forward;
    }

    Carrot::Math::Transform TransformComponent::computeGlobalReactPhysicsTransform() const {
        auto parent = getEntity().getParent();
        if(parent) {
            if (auto parentTransform = getEntity().getWorld().getComponent<TransformComponent>(parent.value())) {
                glm::mat4 finalTransform = toTransformMatrix();

                glm::vec4 globalTranslation{ 0, 0, 0, 1 };
                glm::vec4 forward { 0, 0, -1, 0 };
                glm::vec4 up { 0, 1, 0, 0 }; // warning: up is different from engine default (+Z up)

                globalTranslation = finalTransform * globalTranslation;
                forward = glm::normalize(finalTransform * forward);
                up = glm::normalize(finalTransform * up);

                Carrot::Math::Transform computedGlobalTransform;
                computedGlobalTransform.rotation = glm::quatLookAt(glm::vec3{forward.x, forward.y, forward.z }, glm::vec3{ up.x, up.y, up.z });

                computedGlobalTransform.position = glm::vec3{ globalTranslation.x, globalTranslation.y, globalTranslation.z };
                return computedGlobalTransform;
            }
        }

        return localTransform;
    }

    void TransformComponent::registerUsertype(sol::state& d) {
        d.new_usertype<ECS::TransformComponent>("Transform", sol::no_constructor,
                                                "localTransform", &TransformComponent::localTransform,
                                                "toTransformMatrix", &TransformComponent::toTransformMatrix,
                                                "computeFinalPosition", &TransformComponent::computeFinalPosition
                                                );
    }
}