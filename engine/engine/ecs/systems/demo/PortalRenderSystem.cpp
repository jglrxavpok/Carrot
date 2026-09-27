//
// Created by jglrxavpok on 23/09/2026.
//

#include "PortalRenderSystem.h"

#include <engine/Engine.h>
#include <glm/gtc/type_ptr.inl>

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

    // Based on https://aras-p.info/texts/obliqueortho.html
    static void makeObliqueProjection(glm::mat4& projection, const Math::Plane& clipPlane) {
        glm::vec4 vec4Form = glm::vec4{clipPlane.normal, clipPlane.distanceFromOrigin};
#if 1
        const glm::mat4 invProjection = glm::inverse(projection);
        glm::vec4 q = invProjection * glm::vec4{glm::sign(clipPlane.normal.x), glm::sign(clipPlane.normal.y), 1.0f, 1.0f};

        glm::vec4 c = vec4Form * (2 / glm::dot(vec4Form, q));
       // c.z ++;
        // third row = clip plane - fourth row
        for (int i = 0; i < 4; i++) {
            projection[i][2] = c[i] - projection[i][3];
        }
#else

        // https://terathon.com/blog/oblique-clipping.html
        float* matrix = glm::value_ptr(projection);
        glm::vec4 q = {
            (glm::sign(vec4Form.x) + matrix[8]) / matrix[0],
            (glm::sign(vec4Form.y) + matrix[9]) / matrix[5],
            -1,
            (1 + matrix[10]) / matrix[14]
        };

        glm::vec4 c = vec4Form * (2.0f / glm::dot(vec4Form, q));
        matrix[2] = c.x;
        matrix[6] = c.y;
        matrix[10] = c.z + 1;
        matrix[14] = c.w;
#endif
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
        const glm::vec3 positionRelativeToExitPortal = (right * exitRight + up * exitUp + forward * exitForward);
        const glm::vec3 finalPosition = positionRelativeToExitPortal + exitPos;
        Carrot::Math::Transform finalTransform;
        finalTransform.position = finalPosition;
        finalTransform.rotation = exitRotation * relativeRotation;

        auto modelRotation = glm::toMat4(finalTransform.rotation);
        auto transform = glm::translate(glm::mat4(1.0f), finalTransform.position) * modelRotation;
        glm::mat4 view = glm::inverse(transform);

        glm::mat4 projection = cameraFromMainViewport.getProjectionMatrix();

        const float portalSide = glm::sign(glm::dot(cameraPos - entryPos, entryForward)); // which side of the portal we are exiting from
        const glm::vec3 cameraSpaceExitPortalPos = (view * glm::vec4(exitPos, 1)).xyz();
        const glm::vec3 cameraSpaceExitPortalForward = (glm::transpose(glm::inverse(glm::mat3{view})) * exitForward) * portalSide;
        const float cameraSpaceExitPortalDistance = -glm::dot(cameraSpaceExitPortalForward, cameraSpaceExitPortalPos);
        Math::Plane clipPlane{};
        clipPlane.normal = cameraSpaceExitPortalForward;
        clipPlane.distanceFromOrigin = cameraSpaceExitPortalDistance;

        //makeObliqueProjection(projection, clipPlane);
        viewportCamera.setViewProjection(view, projection);
    }

    std::unique_ptr<Carrot::ECS::System> PortalRenderSystem::duplicate(Carrot::ECS::World& newOwner) const {
        return std::make_unique<PortalRenderSystem>(newOwner);
    }
}
