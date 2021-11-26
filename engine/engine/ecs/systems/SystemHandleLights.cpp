//
// Created by jglrxavpok on 06/03/2021.
//

#include "SystemHandleLights.h"

namespace Carrot::ECS {
    void SystemHandleLights::onFrame(Carrot::Render::Context renderContext) {
        forEachEntity([&](Entity& entity, Transform& transform, LightComponent& light) {
            auto transformMatrix = transform.toTransformMatrix();
            glm::vec3 position = transformMatrix * glm::vec4{0,0,0,1};
            glm::vec3 forward = transformMatrix * glm::vec4{0,1,0,0};
            light.lightRef->light.position = position;
            light.lightRef->light.direction = forward;
        });
    }

    std::unique_ptr<System> SystemHandleLights::duplicate(World& newOwner) const {
        auto system = std::make_unique<SystemHandleLights>(newOwner);
        return system;
    }
}