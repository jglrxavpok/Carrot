#include "PrefabInstanceComponent.h"

namespace Carrot::ECS {
    PrefabInstanceComponent::PrefabInstanceComponent(Carrot::ECS::Entity entity): Carrot::ECS::IdentifiableComponent<PrefabInstanceComponent>(std::move(entity)) {}

    PrefabInstanceComponent::PrefabInstanceComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity): PrefabInstanceComponent(std::move(entity)) {
        TODO;
    }

    rapidjson::Value PrefabInstanceComponent::toJSON(rapidjson::Document& doc) const {
        TODO;
    }

    std::unique_ptr<Carrot::ECS::Component> PrefabInstanceComponent::duplicate(const Carrot::ECS::Entity& newOwner) const {
        auto result = std::make_unique<PrefabInstanceComponent>(newOwner);
        result->isRoot = isRoot;
        result->prefabPath = prefabPath;
        return result;
    }

} // Carrot::ECS