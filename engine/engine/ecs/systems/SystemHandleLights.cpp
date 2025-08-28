//
// Created by jglrxavpok on 06/03/2021.
//

#include "SystemHandleLights.h"

namespace Carrot::ECS {
    void SystemHandleLights::onFrame(const Carrot::Render::Context& renderContext) {
        forEachEntity([&](Entity& entity, TransformComponent& transform, LightComponent& light) {
            auto transformMatrix = transform.toTransformMatrix();
            glm::vec3 position = transformMatrix * glm::vec4{0,0,0,1};
            glm::vec3 forward = transformMatrix * glm::vec4{0,1,0,0};

            switch (light.lightRef->light.type) {
                case Render::LightType::Point:
                    light.lightRef->light.point.position = position;
                    break;
                case Render::LightType::Directional:
                    light.lightRef->light.directional.direction = forward;
                    break;
                case Render::LightType::Spot:
                    light.lightRef->light.spot.position = position;
                    light.lightRef->light.spot.direction = forward;
                    break;
            }

        });
    }

    std::unique_ptr<System> SystemHandleLights::duplicate(World& newOwner) const {
        auto system = std::make_unique<SystemHandleLights>(newOwner);
        return system;
    }

    void SystemHandleLights::reload() {
        forEachEntity([&](Entity& entity, TransformComponent& transform, LightComponent& light) {
            light.reload();
        });
    }

    void SystemHandleLights::unload() {
        forEachEntity([&](Entity& entity, TransformComponent& transform, LightComponent& light) {
            light.unload();
        });
    }
}