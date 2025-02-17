//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/RigidBodyComponent.h>

namespace Carrot::ECS {
    class RigidBodySystem: public LogicSystem<TransformComponent, Carrot::ECS::RigidBodyComponent>, public Identifiable<RigidBodySystem> {
    public:
        explicit RigidBodySystem(World& world);
        explicit RigidBodySystem(const Carrot::DocumentElement& doc, World& world): RigidBodySystem(world) {}

        void tick(double dt) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;

        void reload() override;

        void unload() override;

    public:
        inline static const char* getStringRepresentation() {
            return "RigidBodies";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }

    public:
        static std::optional<Entity> entityFromBody(const Carrot::ECS::World& world, const Physics::RigidBody& body);
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::RigidBodySystem>::getStringRepresentation() {
    return Carrot::ECS::RigidBodySystem::getStringRepresentation();
}