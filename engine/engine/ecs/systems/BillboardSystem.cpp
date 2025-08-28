//
// Created by jglrxavpok on 28/07/2021.
//

#include "BillboardSystem.h"

namespace Carrot::ECS {

    void BillboardSystem::onFrame(const Carrot::Render::Context& renderContext) {
        auto& camera = renderContext.getCamera();
        glm::quat invCameraRotation = glm::inverse(glm::toQuat(camera.getCurrentFrameViewMatrix()));
        forEachEntity([&](Entity& entity, TransformComponent& transform, BillboardComponent& textComponent) {
            glm::quat invParentRotation = glm::inverse(transform.computeFinalOrientation() * glm::inverse(transform.localTransform.rotation));
            //invParentRotation = glm::identity<glm::quat>();
            transform.localTransform.rotation = invParentRotation * invCameraRotation;
        });
    }

    std::unique_ptr<System> BillboardSystem::duplicate(World& newOwner) const {
        return std::make_unique<BillboardSystem>(newOwner);
    }
}
