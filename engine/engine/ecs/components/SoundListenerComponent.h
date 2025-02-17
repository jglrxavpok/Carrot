#pragma once

#include <engine/ecs/components/Component.h>

namespace Carrot::ECS {

    class SoundListenerComponent : public Carrot::ECS::IdentifiableComponent<SoundListenerComponent> {
    public:
        bool active = true;

        explicit SoundListenerComponent(Carrot::ECS::Entity entity);

        explicit SoundListenerComponent(const Carrot::DocumentElement& doc, Carrot::ECS::Entity entity);

        Carrot::DocumentElement serialise() const override {
            Carrot::DocumentElement obj;
            obj["active"] = active;
            return obj;
        }

        const char *const getName() const override {
            return "SoundListener";
        }

        std::unique_ptr<Carrot::ECS::Component> duplicate(const Carrot::ECS::Entity& newOwner) const override;

    private:
    };
}

template<>
inline const char *Carrot::Identifiable<Carrot::ECS::SoundListenerComponent>::getStringRepresentation() {
    return "SoundListener";
}