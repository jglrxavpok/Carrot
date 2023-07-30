#include "NavMeshComponent.h"

namespace Carrot::ECS {
    NavMeshComponent::NavMeshComponent(Carrot::ECS::Entity entity) : Carrot::ECS::IdentifiableComponent<NavMeshComponent>(std::move(entity)) {};

    NavMeshComponent::NavMeshComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity) : NavMeshComponent(std::move(entity)) {
        if(json.HasMember("nav_mesh")) {
            setMesh(json["nav_mesh"].GetString());
        }
    };

    std::unique_ptr <Carrot::ECS::Component> NavMeshComponent::duplicate(const Carrot::ECS::Entity& newOwner) const {
        auto result = std::make_unique<NavMeshComponent>(newOwner);
        if(meshFile.isFile()) {
            result->setMesh(meshFile);
        }
        return result;
    }

    void NavMeshComponent::setMesh(const Carrot::IO::Resource& file) {
        meshFile = file;
        navMesh.loadFromResource(meshFile);
    }

    rapidjson::Value NavMeshComponent::toJSON(rapidjson::Document& doc) const {
        rapidjson::Value obj(rapidjson::kObjectType);

        if(meshFile.isFile()) {
            obj.AddMember("nav_mesh", rapidjson::Value{ meshFile.getName().c_str(), doc.GetAllocator()}, doc.GetAllocator());
        }

        return obj;
    }

} // Carrot::ECS