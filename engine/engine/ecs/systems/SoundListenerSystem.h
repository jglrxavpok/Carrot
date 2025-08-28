#pragma once

#include <cstdint>
#include <functional>
#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/SoundListenerComponent.h>
#include <engine/ecs/components/TransformComponent.h>
#include <core/utils/Lookup.hpp>

namespace Carrot::ECS {
    class SoundListenerSystem
            : public Carrot::ECS::LogicSystem<Carrot::ECS::TransformComponent, Carrot::ECS::SoundListenerComponent>,
              public Carrot::Identifiable<SoundListenerSystem> {
    public:
        explicit SoundListenerSystem(Carrot::ECS::World& world);

        explicit SoundListenerSystem(const Carrot::DocumentElement& doc, Carrot::ECS::World& world) : SoundListenerSystem(
                world) {}

        virtual void tick(double dt) override;

        virtual void onFrame(const Carrot::Render::Context& renderContext) override;

    public:
        inline static const char *getStringRepresentation() {
            return "SoundListenerSystem";
        }

        virtual const char *getName() const override {
            return getStringRepresentation();
        }

        virtual std::unique_ptr<Carrot::ECS::System> duplicate(Carrot::ECS::World& newOwner) const override;

    private:
    };

} // Carrot::ECS

template<>
inline const char *Carrot::Identifiable<Carrot::ECS::SoundListenerSystem>::getStringRepresentation() {
    return Carrot::ECS::SoundListenerSystem::getStringRepresentation();
}