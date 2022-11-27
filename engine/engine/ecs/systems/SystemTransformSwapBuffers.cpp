//
// Created by jglrxavpok on 26/02/2021.
//
#include "SystemTransformSwapBuffers.h"

namespace Carrot::ECS {
    void SystemTransformSwapBuffers::swapBuffers() {
        forEachEntity([&](Entity& entity, TransformComponent& transformComponent) {
            transformComponent.lastFrameGlobalTransform = transformComponent.toTransformMatrix();
        });
    }

    std::unique_ptr<System> SystemTransformSwapBuffers::duplicate(World& newOwner) const {
        return std::make_unique<SystemTransformSwapBuffers>(newOwner);
    }
}