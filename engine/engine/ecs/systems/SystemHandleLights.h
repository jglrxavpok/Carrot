//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/Transform.h"
#include "engine/ecs/components/LightComponent.h"
#include "System.h"

namespace Carrot::ECS {
    class SystemHandleLights: public RenderSystem<Transform, LightComponent> {
    public:
        explicit SystemHandleLights(World& world): RenderSystem<Transform, LightComponent>(world) {}

        void onFrame(Carrot::Render::Context) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;
    };
}


