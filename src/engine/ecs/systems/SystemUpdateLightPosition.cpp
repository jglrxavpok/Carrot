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
}