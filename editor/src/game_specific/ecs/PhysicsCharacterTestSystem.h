#pragma once

#include <cstdint>
#include <functional>
#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/PhysicsCharacterComponent.h>
#include <core/utils/Lookup.hpp>
#include <engine/io/actions/Action.hpp>
#include <engine/io/actions/ActionSet.h>

namespace Peeler::ECS {
    class PhysicsCharacterTestSystem
            : public Carrot::ECS::LogicSystem<Carrot::ECS::TransformComponent, Carrot::ECS::PhysicsCharacterComponent>,
              public Carrot::Identifiable<PhysicsCharacterTestSystem> {
    public:
        explicit PhysicsCharacterTestSystem(Carrot::ECS::World& world);

        explicit PhysicsCharacterTestSystem(const rapidjson::Value& json, Carrot::ECS::World& world)
                : PhysicsCharacterTestSystem(world) {}

        virtual void tick(double dt) override;

        virtual void onFrame(Carrot::Render::Context renderContext) override;

        void reload() override;

        void unload() override;

    public:
        inline static const char *getStringRepresentation() {
            return "Peeler::ECS::PhysicsCharacterTestSystem";
        }

        virtual const char *getName() const override {
            return getStringRepresentation();
        }

        virtual std::unique_ptr<Carrot::ECS::System> duplicate(Carrot::ECS::World& newOwner) const override;

    private:
        Carrot::IO::ActionSet inputSet { "character_controller" };
        Carrot::IO::Vec2InputAction movementInput{ "move" };

        bool firstFrame = true;
    };

} // Peeler::ECS

template<>
inline const char *Carrot::Identifiable<Peeler::ECS::PhysicsCharacterTestSystem>::getStringRepresentation() {
    return Peeler::ECS::PhysicsCharacterTestSystem::getStringRepresentation();
}