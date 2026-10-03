//
// Created by jglrxavpok on 23/09/2026.
//

#include "PortalRenderSystem.h"

#include <engine/Engine.h>
#include <engine/ecs/components/ModelComponent.h>
#include <glm/gtc/type_ptr.inl>

namespace Carrot::ECS {
    static bool freeze = false;

    void PortalRenderSystem::beginFrame(const Carrot::Render::Context& mainRenderContext) {
        if (!regenerateViewports)
            return;

        regenerateViewports = false;
        portalViewportInfo.clear();
        for (Carrot::ECS::EntityID entityID : entities) {
            auto makeViewportIDFromEntryPortal = [](Carrot::ECS::EntityID entity) {
                return Carrot::Identifier { Carrot::sprintf("portal %s", entity.toString().c_str()) };
            };
            Carrot::Identifier id = makeViewportIDFromEntryPortal(entityID);
            Render::Viewport& viewport = GetEngine().getOrCreateViewport(id);

            i32 portalIndex = portalViewportInfo.size();
            Render::StencilSettings viewportStencil{};
            viewportStencil.stencilEnabled = true;
            viewportStencil.stencilCompare = true;
            viewportStencil.stencilValue = portalIndex+1;
            viewportStencil.stencilOperation = Render::StencilOperation::Equal;
            viewport.setStencilSettings(viewportStencil);

            Carrot::ECS::Entity entity = world.wrap(entityID);

            ViewportInfo info {
                .config = viewportStencil,
                .entryPortal = entity,
            };

            portalViewportInfo[id] = info;

            // make exit portal invisible in viewport rendering the view from out of exit portal
            PortalComponent& portalComp = entity.getComponent<PortalComponent>();
            if (portalComp.otherPortal.exists()) {
                portalComp.otherPortal.getComponent<ModelComponent>()->hiddenInViewports = {makeViewportIDFromEntryPortal(entityID)};
            }

            // setup model to write correct value to stencil buffer
            ModelComponent& modelComp = entity.getComponent<ModelComponent>();
            auto pModelRenderer = modelComp.modelRenderer.get();

            // copied from model edition code: ensure all meshes of the model have an override, with the wanted stencil info
            {
                auto cloneRenderer = [&](Carrot::ECS::ModelComponent& component) {
                    if(pModelRenderer) {
                        return pModelRenderer->clone();
                    }

                    return std::make_shared<Carrot::Render::ModelRenderer>(*component.modelResource);
                };

                modelComp.modelRenderer = cloneRenderer(modelComp);
                std::size_t staticMeshCount = modelComp.modelRenderer ? modelComp.modelRenderer->getModel().getStaticMeshes().size() : modelComp.modelResource->getStaticMeshes().size();

                Render::StencilSettings meshStencil = viewportStencil;
                meshStencil.stencilCompare = false;
                meshStencil.stencilWrite = true;
                meshStencil.stencilOperation = Render::StencilOperation::AlwaysPass;
                for(std::size_t i = 0; i < staticMeshCount; i++) {
                    Carrot::Render::MaterialOverride* pExistingOverride = modelComp.modelRenderer->getOverrides().findForMesh(i);
                    if(pExistingOverride) {
                        pExistingOverride->stencilSettings = meshStencil;
                    } else {
                        Carrot::Render::MaterialOverride override;
                        override.meshIndex = i;
                        pExistingOverride->stencilSettings = meshStencil;
                        modelComp.modelRenderer->addOverride(override);
                    }
                }
                modelComp.modelRenderer->recreateStructures();
                world.getWorldData().storeModelRenderer(modelComp.modelRenderer);
            }
        }

        Render::ViewportComposition composition;
        Carrot::Identifier mainViewportID {"main"};
        Render::ViewportLocation& main = composition.viewports[mainViewportID];
        main.stencil.emplace();
        main.stencil->stencilEnabled = true;
        main.stencil->stencilWrite = true;
        main.stencil->stencilValue = 0;
        main.renderingOrder = 0;
        main.z = 1.0f;

        i32 order = 1;
        for (const auto& [id, info] : portalViewportInfo) {
            Render::ViewportLocation& portalView = composition.viewports[id];
            portalView.stencil = info.config;
            portalView.inheritDepthStencil = mainViewportID;
            portalView.z = 0;
            portalView.renderingOrder = order++;
            portalView.discardIfDepthNotWritten = true;
        }

        GetEngine().getGame()->updateViewportComposition(std::move(composition));
    }

    void PortalRenderSystem::reload() {
        //TODO;
    }

    void PortalRenderSystem::unload() {
        //TODO;
    }

    void PortalRenderSystem::onFrame(const Carrot::Render::Context& renderContext) {
        if (renderContext.pViewport == &GetEngine().getMainViewport()) {
            return;
        }
        if (ImGui::Begin("Debug portals")) {
            ImGui::Checkbox("Freeze", &freeze);
        }

        ImGui::End();
    }

    void PortalRenderSystem::onEntityAdded(Entity& entity) {
        regenerateViewports = true;
    }

    void PortalRenderSystem::onEntityRemoved(Entity& entity) {
        regenerateViewports = true;
    }

    // TODO: change signature to avoid copies
    void PortalRenderSystem::setupCamera(Carrot::Render::Context renderContext) {
        if (freeze) {
            return;
        }
        Camera& viewportCamera = renderContext.getCamera();

        auto entryIter = portalViewportInfo.find(renderContext.pViewport->getViewportID());
        if (entryIter == portalViewportInfo.end()) {
            return;
        }

        Carrot::ECS::Entity& entryPortal = entryIter->second.entryPortal;
        if (!entryPortal.exists())
            return;

        PortalComponent& entryPortalComponent = entryPortal.getComponent<PortalComponent>();
        Carrot::ECS::Entity exitPortal = entryPortalComponent.otherPortal;
        if (!exitPortal.exists())
            return;

        const Render::Viewport& mainViewport = GetEngine().getOrCreateViewport(Carrot::Identifier{"main"});
        const Camera& cameraFromMainViewport = mainViewport.getCamera();
        const TransformComponent& entryTransform = entryPortal.getComponent<TransformComponent>();
        const TransformComponent& exitTransform = exitPortal.getComponent<TransformComponent>();
        const glm::vec3 exitPos = exitTransform.computeFinalPosition();
        const glm::vec3 exitForward = exitTransform.computeGlobalForward();

        const glm::vec3 exitRight = glm::cross(exitForward, exitTransform.computeGlobalUp()); // X is forward with current setup

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
