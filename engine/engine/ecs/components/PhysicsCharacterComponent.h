#pragma once

#include <engine/ecs/components/Component.h>
#include <engine/physics/Character.h>

namespace Carrot::ECS {

    class PhysicsCharacterComponent : public Carrot::ECS::IdentifiableComponent<PhysicsCharacterComponent> {
    public:
        Carrot::Physics::Character character;
        bool firstFrame = true;

        explicit PhysicsCharacterComponent(Carrot::ECS::Entity entity);

        explicit PhysicsCharacterComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity);

        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        const char *const getName() const override {
            return "PhysicsCharacter";
        }

        std::unique_ptr<Carrot::ECS::Component> duplicate(const Carrot::ECS::Entity& newOwner) const override;
    };
}

template<>
inline const char *Carrot::Identifiable<Carrot::ECS::PhysicsCharacterComponent>::getStringRepresentation() {
    return "PhysicsCharacter";
}