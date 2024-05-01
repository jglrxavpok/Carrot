//
// Created by jglrxavpok on 16/01/2022.
//

#pragma once

#include <core/containers/Vector.hpp>
#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/RigidBodyComponent.h>
#include <engine/render/Model.h>
#include "engine/render/MaterialSystem.h"

namespace Peeler::ECS {
    class CollisionShapeRenderer: public Carrot::ECS::RenderSystem<Carrot::ECS::TransformComponent, Carrot::ECS::RigidBodyComponent> {
    public:
        explicit CollisionShapeRenderer(Carrot::ECS::World& world);

        void onFrame(Carrot::Render::Context renderContext) override;

        std::unique_ptr<Carrot::ECS::System> duplicate(Carrot::ECS::World& newOwner) const override;

        void setSelected(const Carrot::Vector<Carrot::ECS::EntityID>& selection);

        virtual bool shouldBeSerialized() const override {
            return false;
        }

        virtual const char* getName() const override {
            return "CollisionShapeRenderer";
        }

    private:
        std::shared_ptr<Carrot::Model> sphereMesh;
        std::shared_ptr<Carrot::Pipeline> pipeline;
        std::shared_ptr<Carrot::Render::MaterialHandle> material;
        Carrot::Vector<Carrot::UUID> selectedIDs;
    };
}
