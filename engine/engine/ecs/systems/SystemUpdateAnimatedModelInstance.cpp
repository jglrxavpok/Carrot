//
// Created by jglrxavpok on 27/02/2021.
//

#include <engine/ecs/components/Transform.h>
#include "SystemUpdateAnimatedModelInstance.h"

namespace Carrot::ECS {
    void SystemUpdateAnimatedModelInstance::onFrame(Carrot::Render::Context renderContext) {
        forEachEntity([&](Entity& entity, Transform& transform, AnimatedModelInstance& model) {
            model.instanceData.transform = transform.toTransformMatrix();
        });
    }

    void SystemUpdateAnimatedModelInstance::tick(double dt) {
        forEachEntity([&](Entity& entity, Transform& transform, AnimatedModelInstance& model) {
            model.instanceData.animationTime += dt;
        });
    }

    void SystemUpdateAnimatedModelInstance::onEntityAdded(Entity& e) {
        AnimatedModelInstance& model = world.getComponent<AnimatedModelInstance>(e);
        model.instanceData.animationIndex = rand() % 2;
        //model->instanceData.animationTime = rand() / 10.0f;
    }

    std::unique_ptr<System> SystemUpdateAnimatedModelInstance::duplicate(World& newOwner) const {
        auto system = std::make_unique<SystemUpdateAnimatedModelInstance>(newOwner);
        return system;
    }
}