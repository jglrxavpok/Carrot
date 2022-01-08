//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/RigidBodyComponent.h>

namespace Carrot::ECS {
    class RigidBodySystem: public LogicSystem<TransformComponent, Carrot::ECS::RigidBodyComponent> {
    public:
        explicit RigidBodySystem(World& world);

        void tick(double dt) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;

        void reload() override;

        void unload() override;
    };
}
