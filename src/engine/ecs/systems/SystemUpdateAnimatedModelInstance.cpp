//
// Created by jglrxavpok on 27/02/2021.
//

#include <engine/ecs/components/Transform.h>
#include "SystemUpdateAnimatedModelInstance.h"

void Carrot::SystemUpdateAnimatedModelInstance::onFrame(size_t frameIndex) {
    for(const auto& e : entities) {
        if(auto entity = e.lock()) {
            auto* transform = world.getComponent<Transform>(entity);
            auto* model = world.getComponent<AnimatedModelInstance>(entity);

            model->instanceData.transform = transform->toTransformMatrix();
        }
    }
}

void Carrot::SystemUpdateAnimatedModelInstance::tick(double dt) {
    for(const auto& e : entities) {
        if(auto entity = e.lock()) {
            auto* model = world.getComponent<AnimatedModelInstance>(entity);

            model->instanceData.animationTime += static_cast<float>(dt);
        }
    }
}

void Carrot::SystemUpdateAnimatedModelInstance::onEntityAdded(Entity_WeakPtr e) {
    if(auto entity = e.lock()) {
        auto* model = world.getComponent<AnimatedModelInstance>(entity);
        model->instanceData.animationIndex = rand() % 2;
        model->instanceData.animationTime = rand() / 10.0f;
    }
}
