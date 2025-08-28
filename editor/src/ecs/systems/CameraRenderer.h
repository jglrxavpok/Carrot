//
// Created by jglrxavpok on 20/02/2022.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/CameraComponent.h>
#include <engine/render/Model.h>
#include "engine/render/MaterialSystem.h"
#include "engine/render/AsyncResource.hpp"

namespace Peeler::ECS {
    class CameraRenderer: public Carrot::ECS::RenderSystem<Carrot::ECS::TransformComponent, Carrot::ECS::CameraComponent> {
    public:
        explicit CameraRenderer(Carrot::ECS::World& world);

        void onFrame(const Carrot::Render::Context& renderContext) override;

        std::unique_ptr<Carrot::ECS::System> duplicate(Carrot::ECS::World& newOwner) const override;

        virtual bool shouldBeSerialized() const override {
            return false;
        }

        virtual const char* getName() const override {
            return "CameraRenderer";
        }

    private:
        Carrot::AsyncModelResource cameraModel;
        std::shared_ptr<Carrot::Pipeline> primaryCameraPipeline = nullptr;
        std::shared_ptr<Carrot::Pipeline> secondaryCameraPipeline = nullptr;
    };
}
