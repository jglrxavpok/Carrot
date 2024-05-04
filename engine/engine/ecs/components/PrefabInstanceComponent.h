#pragma once

#include <core/io/vfs/VirtualFileSystem.h>
#include <engine/ecs/components/Component.h>

namespace Carrot::ECS {

    class PrefabInstanceComponent: public Carrot::ECS::IdentifiableComponent<PrefabInstanceComponent> {
    public:
        std::shared_ptr<const Carrot::ECS::Prefab> prefab;
        bool isRoot = false;

        explicit PrefabInstanceComponent(Carrot::ECS::Entity entity);

        explicit PrefabInstanceComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity);

        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        const char *const getName() const override {
            return Carrot::Identifiable<Carrot::ECS::PrefabInstanceComponent>::getStringRepresentation();
        }

        std::unique_ptr<Carrot::ECS::Component> duplicate(const Carrot::ECS::Entity& newOwner) const override;

    private:
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::PrefabInstanceComponent>::getStringRepresentation() {
    return "Carrot::ECS::PrefabInstanceComponent";
}