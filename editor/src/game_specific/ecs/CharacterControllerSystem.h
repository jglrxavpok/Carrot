//
// Created by jglrxavpok on 24/03/2022.
//

#pragma once

#include <engine/ecs/World.h>
#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/RigidBodyComponent.h>
#include <engine/io/actions/ActionSet.h>
#include <engine/io/actions/Action.hpp>
#include "CharacterControllerComponent.h"

namespace Game::ECS {
    class CharacterControllerSystem: public Carrot::ECS::LogicSystem<Carrot::ECS::RigidBodyComponent, Game::ECS::CharacterControllerComponent>, public Carrot::Identifiable<CharacterControllerSystem> {
    public:
        explicit CharacterControllerSystem(Carrot::ECS::World& world);

        explicit CharacterControllerSystem(const rapidjson::Value& json, Carrot::ECS::World& world): CharacterControllerSystem(world) {}

        void tick(double deltaTime) override;

        std::unique_ptr<Carrot::ECS::System> duplicate(Carrot::ECS::World& newOwner) const override;

        void reload() override;

        void unload() override;

    public:
        inline static const char* getStringRepresentation() {
            return "CharacterControllers";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }

    private:
        Carrot::IO::Vec2InputAction movementInput{ "Move" };
        Carrot::IO::Vec2InputAction lookInput{ "Look" };

        Carrot::IO::ActionSet inputSet { "Character Controller" };

        float speedFactor = 60.0f;
    };
}