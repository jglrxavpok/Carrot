//
// Created by jglrxavpok on 20/02/2022.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include "engine/ecs/components/CameraComponent.h"

namespace Carrot::ECS {
    class CameraSystem
            : public RenderSystem<TransformComponent, Carrot::ECS::CameraComponent>,
              public Identifiable<CameraSystem> {
    public:
        explicit CameraSystem(World& world): RenderSystem<TransformComponent, CameraComponent>(world) {}

        explicit CameraSystem(const rapidjson::Value& json, World& world): CameraSystem(world) {}

        void onFrame(Carrot::Render::Context renderContext) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override {
            return std::make_unique<CameraSystem>(newOwner);
        };

    public:
        inline static const char *getStringRepresentation() {
            return "PrimaryCamera";
        }

        virtual const char *getName() const override {
            return getStringRepresentation();
        }

    };
}
