//
// Created by jglrxavpok on 27/02/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/AnimatedModelInstance.h"
#include "engine/ecs/systems/System.h"

namespace Carrot::ECS {
    class SystemUpdateAnimatedModelInstance: public RenderSystem<Transform, AnimatedModelInstance> {
    public:
        explicit SystemUpdateAnimatedModelInstance(World& world): RenderSystem<Transform, AnimatedModelInstance>(world) {}

        void onFrame(Carrot::Render::Context renderContext) override;

        void tick(double dt) override;

    protected:
        void onEntityAdded(Entity& entity) override;
    };

}
