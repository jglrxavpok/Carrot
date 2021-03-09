//
// Created by jglrxavpok on 06/03/2021.
//

#include "SystemUpdateLightPosition.h"

void Carrot::SystemUpdateLightPosition::tick(double dt) {
    for (const auto& e : entities) {
        if(auto entity = e.lock()) {
            auto* transform = world.getComponent<Transform>(entity);
            auto* light = world.getComponent<RaycastedShadowsLight>(entity);

            light->lightRef.position = transform->position;
        }
    }
}