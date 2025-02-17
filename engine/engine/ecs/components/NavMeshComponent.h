#pragma once

#include <engine/ecs/components/Component.h>
#include <engine/pathfinding/NavMesh.h>

namespace Carrot::ECS {

    class NavMeshComponent : public Carrot::ECS::IdentifiableComponent<NavMeshComponent> {
    public:
        Carrot::AI::NavMesh navMesh;
        Carrot::IO::Resource meshFile;

        explicit NavMeshComponent(Carrot::ECS::Entity entity);

        explicit NavMeshComponent(const Carrot::DocumentElement& json, Carrot::ECS::Entity entity);

        Carrot::DocumentElement serialise() const override;

        void setMesh(const Carrot::IO::Resource& file);

        const char *const getName() const override {
            return "NavMesh";
        }

        std::unique_ptr<Carrot::ECS::Component> duplicate(const Carrot::ECS::Entity& newOwner) const override;

    private:
    };
}

template<>
inline const char *Carrot::Identifiable<Carrot::ECS::NavMeshComponent>::getStringRepresentation() {
    return "NavMesh";
}