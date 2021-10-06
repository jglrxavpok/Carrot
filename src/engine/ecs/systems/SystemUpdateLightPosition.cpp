//
// Created by jglrxavpok on 06/03/2021.
//

#include "SystemUpdateLightPosition.h"

namespace Carrot::ECS {
    void SystemUpdateLightPosition::tick(double dt) {
        forEachEntity([&](Entity& entity, Transform& transform, RaycastedShadowsLight& light) {
            light.lightRef.position = transform.position;
        });
    }

    std::unique_ptr<System> SystemUpdateLightPosition::duplicate(World& newOwner) const {
        auto system = std::make_unique<SystemUpdateLightPosition>(newOwner);
        return system;
    }
}