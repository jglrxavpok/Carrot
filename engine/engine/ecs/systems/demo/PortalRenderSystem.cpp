//
// Created by jglrxavpok on 23/09/2026.
//

#include "PortalRenderSystem.h"

#include <engine/Engine.h>
#include <glm/gtc/type_ptr.inl>

namespace Carrot::ECS {
    static bool freeze = false;

    void PortalRenderSystem::onFrame(const Carrot::Render::Context& renderContext) {
        if (renderContext.pViewport->getViewportID() != Carrot::Identifier{"portal1"}) {
            return;
        }
        if (ImGui::Begin("Debug portals")) {
            ImGui::Checkbox("Freeze", &freeze);
        }

        ImGui::End();
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
        if (freeze) {
            return;
        }
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
        const TransformComponent& entryTransform = entryPortal.getComponent<TransformComponent>();
        const TransformComponent& exitTransform = exitPortal.getComponent<TransformComponent>();
        const glm::vec3 exitPos = exitTransform.computeFinalPosition();
        const glm::vec3 exitForward = exitTransform.computeGlobalForward();

        const glm::vec3 exitRight = glm::cross(exitForward, exitTransform.computeGlobalUp());

        glm::mat4 finalTransformAsMatrix = exitTransform.toTransformMatrix() * glm::inverse(entryTransform.toTransformMatrix()) * glm::inverse(cameraFromMainViewport.getCurrentFrameViewMatrix());

        Math::Transform finalTransform;
        finalTransform.position = finalTransformAsMatrix[3].xyz();
        finalTransform.rotation = glm::quat(glm::mat3{finalTransformAsMatrix});

        glm::mat4 view = glm::inverse(finalTransform.toTransformMatrix());
        glm::mat4 projection = cameraFromMainViewport.getProjectionMatrix();
        viewportCamera.setViewProjection(view, projection);

        Math::Plane clipPlane{};
        float portalSide = glm::sign(glm::dot(exitPos - finalTransform.position, exitRight));
        clipPlane.normal = exitRight * portalSide;
        clipPlane.distanceFromOrigin = -glm::dot(clipPlane.normal, exitPos);
        viewportCamera.setWorldSpaceClippingPlane(clipPlane);
    }

    std::unique_ptr<Carrot::ECS::System> PortalRenderSystem::duplicate(Carrot::ECS::World& newOwner) const {
        return std::make_unique<PortalRenderSystem>(newOwner);
    }
}
