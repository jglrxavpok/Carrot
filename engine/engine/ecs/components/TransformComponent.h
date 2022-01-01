//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include "Component.h"
#include <engine/math/Transform.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Carrot::ECS {
    struct TransformComponent: public IdentifiableComponent<TransformComponent> {
        Carrot::Math::Transform localTransform;

        explicit TransformComponent(Entity entity): IdentifiableComponent<TransformComponent>(std::move(entity)) {};

        explicit TransformComponent(const rapidjson::Value& json, Entity entity);

        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        [[nodiscard]] glm::mat4 toTransformMatrix() const;

        const char *const getName() const override {
            return "Transform";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<TransformComponent>(newOwner);
            result->localTransform = localTransform;
            return result;
        }

        void drawInspectorInternals(const Render::Context& renderContext, bool& modified) override;

        glm::vec3 computeFinalPosition() const;

        /// Computes the global transform of this entity, taking into account the hierarchy
        Carrot::Math::Transform computeGlobalTransform() const;

        /// Sets up the transform of the entity to match with the given transform, even when parent transforms are taken into account
        void setGlobalTransform(const Carrot::Math::Transform& transform);
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::TransformComponent>::getStringRepresentation() {
    return "Transform";
}
