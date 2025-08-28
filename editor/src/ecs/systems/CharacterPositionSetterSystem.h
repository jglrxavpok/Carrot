//
// Created by jglrxavpok on 28/07/2023.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/PhysicsCharacterComponent.h>
#include <engine/render/Model.h>
#include "engine/render/MaterialSystem.h"

namespace Peeler::ECS {
    /// Updates transform of CharacterComponent while game is in editing, to make sure debug shows collisions at the proper location
    class CharacterPositionSetterSystem: public Carrot::ECS::RenderSystem<Carrot::ECS::TransformComponent, Carrot::ECS::PhysicsCharacterComponent> {
    public:
        explicit CharacterPositionSetterSystem(Carrot::ECS::World& world);

        void onFrame(const Carrot::Render::Context& renderContext) override;

        std::unique_ptr<Carrot::ECS::System> duplicate(Carrot::ECS::World& newOwner) const override;

        virtual bool shouldBeSerialized() const override {
            return false;
        }

        virtual const char* getName() const override {
            return "CharacterPositionSetterSystem";
        }

    private:
    };
}
