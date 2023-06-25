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
            result->wasActive = wasActive;
            result->firstTick = firstTick;
            return result;
        }

        void reload();
        void unload();

        static const char* getTypeName(Physics::BodyType type);
        static Physics::BodyType getTypeFromName(const std::string& name);

    private:
        bool firstTick = true;
        bool wasActive = true;

        friend class RigidBodySystem;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::RigidBodyComponent>::getStringRepresentation() {
    return "RigidBodyComponent";
}