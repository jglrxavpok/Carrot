//
// Created by jglrxavpok on 27/02/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/AnimatedModelInstance.h"
#include "engine/ecs/systems/System.h"

namespace Carrot::ECS {
    class SystemUpdateAnimatedModelInstance: public RenderSystem<TransformComponent, AnimatedModelInstance> {
    public:
        explicit SystemUpdateAnimatedModelInstance(World& world): RenderSystem<TransformComponent, AnimatedModelInstance>(world) {}

        void onFrame(Carrot::Render::Context renderContext) override;

        void tick(double dt) override;

    protected:
        void onEntityAdded(Entity& entity) override;

    public:
        std::unique_ptr<System> duplicate(World& newOwner) const override;
    };

}
