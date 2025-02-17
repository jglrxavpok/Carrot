#include "NavMeshComponent.h"

namespace Carrot::ECS {
    NavMeshComponent::NavMeshComponent(Carrot::ECS::Entity entity) : Carrot::ECS::IdentifiableComponent<NavMeshComponent>(std::move(entity)) {};

    NavMeshComponent::NavMeshComponent(const Carrot::DocumentElement& doc, Carrot::ECS::Entity entity) : NavMeshComponent(std::move(entity)) {
        if(doc.contains("nav_mesh")) {
            setMesh(Carrot::IO::Resource{ doc["nav_mesh"].getAsString() });
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

    Carrot::DocumentElement NavMeshComponent::serialise() const {
        Carrot::DocumentElement obj;

        if(meshFile.isFile()) {
            obj["nav_mesh"] = meshFile.getName();
        }

        return obj;
    }

} // Carrot::ECS