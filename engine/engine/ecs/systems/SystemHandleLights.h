//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/LightComponent.h"
#include "System.h"

namespace Carrot::ECS {
    class SystemHandleLights: public RenderSystem<TransformComponent, LightComponent> {
    public:
        explicit SystemHandleLights(World& world): RenderSystem<TransformComponent, LightComponent>(world) {}

        void onFrame(Carrot::Render::Context) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;
    };
}


