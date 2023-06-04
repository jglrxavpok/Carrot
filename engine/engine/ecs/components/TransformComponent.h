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

        /// used for motion vectors
        glm::mat4 lastFrameGlobalTransform = glm::mat4 {0.0f};

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

        /**
         * Computes the final position of the entity based on the parent orientation & position and this entity's local transform.
         * @return World-space position of the entity
         */
        glm::vec3 computeFinalPosition() const;

        /**
         * Computes the final scale of the entity based on the parent scale and this entity's local transform.
         * @return World-space scale of the entity
         */
        glm::vec3 computeFinalScale() const;

        /**
         * Computes the final orientation of the entity based on the parent orientation and this entity's orientation.
         * @return World-space orientation of the entity
         */
        glm::quat computeFinalOrientation() const;

        /**
        * Computes the global forward of the entity based on the parent orientation and this entity's orientation.
        * @return World-space forward of the entity
        */
        glm::vec3 computeGlobalForward() const;

        /// Computes the global transform of this entity, taking into account the hierarchy
        Carrot::Math::Transform computeGlobalReactPhysicsTransform() const;

        /// Sets up the transform of the entity to match with the given transform, even when parent transforms are taken into account
        void setGlobalTransform(const Carrot::Math::Transform& transform);

    public:
        static void registerUsertype(sol::state& destination);
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::TransformComponent>::getStringRepresentation() {
    return "Transform";
}
