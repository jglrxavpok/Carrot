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
        Carrot::Math::Transform transform;

        explicit TransformComponent(Entity entity): IdentifiableComponent<TransformComponent>(std::move(entity)) {};

        explicit TransformComponent(const rapidjson::Value& json, Entity entity);

        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        [[nodiscard]] glm::mat4 toTransformMatrix() const;

        const char *const getName() const override {
            return "Transform";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<TransformComponent>(newOwner);
            result->transform = transform;
            return result;
        }

        void drawInspectorInternals(const Render::Context& renderContext, bool& modified) override;

        glm::vec3 computeFinalPosition() const;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::TransformComponent>::getStringRepresentation() {
    return "Transform";
}
