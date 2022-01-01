//
// Created by jglrxavpok on 31/12/2021.
//

#pragma once

#include "Component.h"
#include <engine/physics/RigidBody.h>

namespace Carrot::ECS {
    struct RigidBodyComponent: IdentifiableComponent<RigidBodyComponent> {
        Physics::RigidBody rigidbody;

        explicit RigidBodyComponent(Entity entity): IdentifiableComponent<RigidBodyComponent>(std::move(entity)) {}

        explicit RigidBodyComponent(const rapidjson::Value& json, Entity entity);

        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        const char *const getName() const override {
            return "RigidBodyComponent";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<RigidBodyComponent>(newOwner);
            result->rigidbody = rigidbody;
            return result;
        }

        void drawInspectorInternals(const Render::Context& renderContext, bool& modified) override;

    private:
        static const char* getTypeName(reactphysics3d::BodyType type);
        static reactphysics3d::BodyType getTypeFromName(const std::string& name);

    private:
        bool firstTick = true;

        friend class RigidBodySystem;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::RigidBodyComponent>::getStringRepresentation() {
    return "RigidBodyComponent";
}