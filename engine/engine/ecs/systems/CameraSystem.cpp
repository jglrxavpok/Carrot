//
// Created by jglrxavpok on 20/02/2022.
//

#include "CameraSystem.h"
#include "engine/ecs/components/TransformComponent.h"

namespace Carrot::ECS {
    void CameraSystem::setupCamera(Carrot::Render::Context renderContext) {
        auto& camera = renderContext.getCamera();
        forEachEntity([&](Entity& entity, TransformComponent& transform, CameraComponent& cameraComponent) {
            if(!cameraComponent.isPrimary) {
                return;
            }
            auto viewMatrix = transform.toTransformMatrix();
            viewMatrix = glm::inverse(viewMatrix);

            camera.setViewProjection(viewMatrix, cameraComponent.makeProjectionMatrix(*renderContext.pViewport));
        });
    }

    void CameraSystem::onFrame(const Carrot::Render::Context& renderContext) {
        // no op
    }
}