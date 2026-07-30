//
// Created by jglrxavpok on 06/03/2021.
//

#include "SystemHandleLights.h"

#include <engine/scene/Scene.h>

namespace Carrot::ECS {
    void SystemHandleLights::onFrame(const Carrot::Render::Context& renderContext) {
        parallelForEachEntity([&](Entity& entity, TransformComponent& transform, LightComponent& lightComp) {
            auto transformMatrix = transform.toTransformMatrix();
            glm::vec3 position = transformMatrix * glm::vec4{0,0,0,1};
            glm::vec3 forward = transformMatrix * glm::vec4{0,1,0,0};

            Render::Light& light = *lightComp.lightRef;
            switch (light.type) {
                case Render::LightType::Point:
                    light.point.position = position;
                    break;
                case Render::LightType::Directional:
                    light.directional.direction = forward;
                    break;
                case Render::LightType::Spot:
                    light.spot.position = position;
                    light.spot.direction = forward;
                    break;
            }

            world.getLighting().writeToGPU(lightComp.lightRef, renderContext);
        });
    }

    std::unique_ptr<System> SystemHandleLights::duplicate(World& newOwner) const {
        auto system = std::make_unique<SystemHandleLights>(newOwner);
        return system;
    }
}
