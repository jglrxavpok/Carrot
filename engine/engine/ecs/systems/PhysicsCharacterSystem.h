#pragma once

#include <cstdint>
#include <functional>
#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/PhysicsCharacterComponent.h>
#include <core/utils/Lookup.hpp>
#include <engine/io/actions/Action.hpp>
#include <engine/io/actions/ActionSet.h>

namespace Carrot::ECS {
    class PhysicsCharacterSystem
            : public Carrot::ECS::LogicSystem<Carrot::ECS::TransformComponent, Carrot::ECS::PhysicsCharacterComponent>,
              public Carrot::Identifiable<PhysicsCharacterSystem> {
    public:
        explicit PhysicsCharacterSystem(Carrot::ECS::World& world);

        explicit PhysicsCharacterSystem(const Carrot::DocumentElement& doc, Carrot::ECS::World& world)
                : PhysicsCharacterSystem(world) {}

        virtual void tick(double dt) override;
        virtual void prePhysics() override;
        virtual void postPhysics() override;

        virtual void onFrame(const Carrot::Render::Context& renderContext) override;

        void reload() override;

        void unload() override;

    public:
        inline static const char *getStringRepresentation() {
            return "PhysicsCharacters";
        }

        virtual const char *getName() const override {
            return getStringRepresentation();
        }

        virtual std::unique_ptr<Carrot::ECS::System> duplicate(Carrot::ECS::World& newOwner) const override;
    };

} // Peeler::ECS

template<>
inline const char *Carrot::Identifiable<Carrot::ECS::PhysicsCharacterSystem>::getStringRepresentation() {
    return Carrot::ECS::PhysicsCharacterSystem::getStringRepresentation();
}