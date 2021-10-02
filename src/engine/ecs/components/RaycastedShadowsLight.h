//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once

#include "Component.h"
#include <engine/render/lighting/Lights.h>

namespace Carrot::ECS {
    struct RaycastedShadowsLight: public IdentifiableComponent<RaycastedShadowsLight> {
        Light& lightRef;

        explicit RaycastedShadowsLight(Entity entity, Light& light): IdentifiableComponent<RaycastedShadowsLight>(std::move(entity)), lightRef(light) {
            light.enabled = true;
        };

        const char *const getName() const override {
            return "RaycastedShadowsLight";
        }
    };
}