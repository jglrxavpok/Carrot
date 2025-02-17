#include "PrefabInstanceComponent.h"

#include <engine/assets/AssetServer.h>
#include <engine/ecs/Prefab.h>
#include <engine/utils/Macros.h>

namespace Carrot::ECS {
    PrefabInstanceComponent::PrefabInstanceComponent(Carrot::ECS::Entity entity): Carrot::ECS::IdentifiableComponent<PrefabInstanceComponent>(std::move(entity)) {}

    PrefabInstanceComponent::PrefabInstanceComponent(const Carrot::DocumentElement& doc, Carrot::ECS::Entity entity): PrefabInstanceComponent(std::move(entity)) {
        if(doc.contains("prefab_path")) {
            auto& pathObj = doc["prefab_path"];
            Carrot::IO::VFS::Path vfsPath { pathObj.getAsString() };
            prefab = GetAssetServer().blockingLoadPrefab(vfsPath);
            childID = Carrot::UUID::fromString(doc["child_id"].getAsString());
        }
    }

    Carrot::DocumentElement PrefabInstanceComponent::serialise() const {
        if(prefab) {
            Carrot::DocumentElement obj;
            obj["prefab_path"] = prefab->getVFSPath().toString();
            obj["child_id"] = childID.toString();
            return obj;
        } else {
            return Carrot::DocumentElement{};
        }
    }

    std::unique_ptr<Carrot::ECS::Component> PrefabInstanceComponent::duplicate(const Carrot::ECS::Entity& newOwner) const {
        auto result = std::make_unique<PrefabInstanceComponent>(newOwner);
        result->childID = childID;
        result->prefab = prefab;
        return result;
    }

} // Carrot::ECS