//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Carrot::ECS {
    struct Transform: public IdentifiableComponent<Transform> {
        glm::vec3 position{};
        glm::vec3 scale{1.0f};
        glm::quat rotation = glm::identity<glm::quat>();

        explicit Transform(Entity entity): IdentifiableComponent<Transform>(std::move(entity)) {};

        explicit Transform(const rapidjson::Value& json, Entity entity);

        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        [[nodiscard]] glm::mat4 toTransformMatrix() const;

        const char *const getName() const override {
            return "Transform";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<Transform>(newOwner);
            result->position = position;
            result->scale = scale;
            result->rotation = rotation;
            return result;
        }

        void drawInspectorInternals(const Render::Context& renderContext, bool& modified) override;

        glm::vec3 computeFinalPosition() const;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::Transform>::getStringRepresentation() {
    return "Transform";
}
