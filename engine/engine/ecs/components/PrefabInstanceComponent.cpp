#include "PrefabInstanceComponent.h"

#include <engine/assets/AssetServer.h>
#include <engine/ecs/Prefab.h>
#include <engine/utils/Macros.h>

namespace Carrot::ECS {
    PrefabInstanceComponent::PrefabInstanceComponent(Carrot::ECS::Entity entity): Carrot::ECS::IdentifiableComponent<PrefabInstanceComponent>(std::move(entity)) {}

    PrefabInstanceComponent::PrefabInstanceComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity): PrefabInstanceComponent(std::move(entity)) {
        if(json.HasMember("prefab_path")) {
            auto& pathObj = json["prefab_path"];
            Carrot::IO::VFS::Path vfsPath = std::string_view { pathObj.GetString(), pathObj.GetStringLength() };
            prefab = GetAssetServer().blockingLoadPrefab(vfsPath);
            isRoot = json["is_root"].GetBool();
        }
    }

    rapidjson::Value PrefabInstanceComponent::toJSON(rapidjson::Document& doc) const {
        if(prefab) {
            rapidjson::Value obj { rapidjson::kObjectType };
            obj.AddMember("prefab_path", rapidjson::Value { prefab->getVFSPath().toString(), doc.GetAllocator() }, doc.GetAllocator());
            obj.AddMember("is_root", isRoot, doc.GetAllocator());
            return obj;
        } else {
            return {};
        }
    }

    std::unique_ptr<Carrot::ECS::Component> PrefabInstanceComponent::duplicate(const Carrot::ECS::Entity& newOwner) const {
        auto result = std::make_unique<PrefabInstanceComponent>(newOwner);
        result->isRoot = isRoot;
        result->prefab = prefab;
        return result;
    }

} // Carrot::ECS