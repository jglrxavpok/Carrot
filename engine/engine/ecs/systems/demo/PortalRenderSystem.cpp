//
// Created by jglrxavpok on 23/09/2026.
//

#include "PortalRenderSystem.h"

#include <engine/Engine.h>

namespace Carrot::ECS {
    void PortalRenderSystem::onFrame(const Carrot::Render::Context& renderContext) {

    }

    void PortalRenderSystem::onEntityAdded(Entity& entity) {
        if (entity.getName() == "Portal1") {
            entryPortal = entity;
        } else if (entity.getName() == "Portal2") {
            exitPortal = entity;
        }
    }

    // TODO: change signature to avoid copies
    void PortalRenderSystem::setupCamera(Carrot::Render::Context renderContext) {
        Camera& viewportCamera = renderContext.getCamera();

        if (renderContext.pViewport->getViewportID() != Carrot::Identifier{"portal1"}) {
            return;
        }

        if (!entryPortal.exists())
            return;
        if (!exitPortal.exists())
            return;

        const Render::Viewport& mainViewport = GetEngine().getOrCreateViewport(Carrot::Identifier{"main"});
        const Camera& cameraFromMainViewport = mainViewport.getCamera();

        const glm::vec3 cameraPos = cameraFromMainViewport.computePosition();
        const glm::quat cameraQuat = cameraFromMainViewport.computeOrientation();
        const TransformComponent& entryTransform = entryPortal.getComponent<TransformComponent>();
        const TransformComponent& exitTransform = exitPortal.getComponent<TransformComponent>();
        const glm::vec3 entryPos = entryTransform.computeFinalPosition();
        const glm::vec3 exitPos = exitTransform.computeFinalPosition();
        const glm::quat entryRotation = entryTransform.computeFinalOrientation();
        const glm::quat exitRotation = exitTransform.computeFinalOrientation();

        glm::vec3 entryUp, entryForward;
        glm::vec3 exitUp, exitForward;
        entryTransform.computeGlobalUpForward(entryUp, entryForward);
        exitTransform.computeGlobalUpForward(exitUp, exitForward);
        glm::vec3 entryRight = glm::cross(entryForward, entryUp);
        glm::vec3 exitRight = glm::cross(exitForward, exitUp);

        // get position related to entry portal
        const glm::vec3 cameraToEntryPortal = entryPos - cameraPos;
        float right = glm::dot(cameraToEntryPortal, entryRight);
        float up = glm::dot(cameraToEntryPortal, entryUp);
        float forward = glm::dot(cameraToEntryPortal, entryForward);

        glm::quat relativeRotation = cameraQuat * glm::inverse(entryRotation);

        right = -right;
        up = -up;
        forward = -forward;
        //relativeRotation = glm::inverse(relativeRotation);

        // transform to position relative to exit portal
        const glm::vec3 positionRelativeToExitPortal = (right * exitRight + up * exitUp + forward * exitForward)/2.0f;
        const glm::vec3 finalPosition = positionRelativeToExitPortal + exitPos;
        Carrot::Math::Transform finalTransform;
        finalTransform.position = finalPosition;
        finalTransform.rotation = exitRotation * relativeRotation;

        auto modelRotation = glm::toMat4(finalTransform.rotation);
        auto transform = glm::translate(glm::mat4(1.0f), finalTransform.position) * modelRotation;
        glm::mat4 view = glm::inverse(transform);

        viewportCamera.setViewProjection(view, cameraFromMainViewport.getProjectionMatrix());
        // TODO: different clip plane
    }

    std::unique_ptr<Carrot::ECS::System> PortalRenderSystem::duplicate(Carrot::ECS::World& newOwner) const {
        return std::make_unique<PortalRenderSystem>(newOwner);
    }
}
