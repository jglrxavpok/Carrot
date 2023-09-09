#include "SoundListenerComponent.h"

namespace Carrot::ECS {
    SoundListenerComponent::SoundListenerComponent(Carrot::ECS::Entity entity)
            : Carrot::ECS::IdentifiableComponent<SoundListenerComponent>(std::move(entity)) {}

    SoundListenerComponent::SoundListenerComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity)
            : SoundListenerComponent(std::move(entity)) {
        active = json["active"].GetBool();
    };

    std::unique_ptr <Carrot::ECS::Component> SoundListenerComponent::duplicate(const Carrot::ECS::Entity& newOwner) const {
        auto result = std::make_unique<SoundListenerComponent>(newOwner);
        result->active = active;
        return result;
    }

} // Carrot::ECS