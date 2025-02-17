#include "SoundListenerComponent.h"

namespace Carrot::ECS {
    SoundListenerComponent::SoundListenerComponent(Carrot::ECS::Entity entity)
            : Carrot::ECS::IdentifiableComponent<SoundListenerComponent>(std::move(entity)) {}

    SoundListenerComponent::SoundListenerComponent(const Carrot::DocumentElement& doc, Carrot::ECS::Entity entity)
            : SoundListenerComponent(std::move(entity)) {
        active = doc["active"].getAsBool();
    };

    std::unique_ptr <Carrot::ECS::Component> SoundListenerComponent::duplicate(const Carrot::ECS::Entity& newOwner) const {
        auto result = std::make_unique<SoundListenerComponent>(newOwner);
        result->active = active;
        return result;
    }

} // Carrot::ECS