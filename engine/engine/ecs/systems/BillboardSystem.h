//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/BillboardComponent.h>

namespace Carrot::ECS {
    class BillboardSystem: public RenderSystem<TransformComponent, Carrot::ECS::BillboardComponent>, public Identifiable<BillboardSystem> {
    public:
        explicit BillboardSystem(World& world): RenderSystem<TransformComponent, BillboardComponent>(world) {}
        explicit BillboardSystem(const rapidjson::Value& json, World& world): BillboardSystem(world) {}

        void onFrame(Carrot::Render::Context renderContext) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;

    public:
        inline static const char* getStringRepresentation() {
            return "Billboard";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::BillboardSystem>::getStringRepresentation() {
    return Carrot::ECS::BillboardSystem::getStringRepresentation();
}