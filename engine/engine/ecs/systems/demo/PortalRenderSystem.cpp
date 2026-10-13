//
// Created by jglrxavpok on 23/09/2026.
//

#include "PortalRenderSystem.h"

#include <engine/Engine.h>
#include <engine/ecs/components/ModelComponent.h>
#include <glm/gtc/type_ptr.inl>

namespace Carrot::ECS {
    static bool freeze = false;

    const Carrot::Identifier MainViewportID {"main"};

    static Carrot::Identifier makeViewportIDFromEntryPortal(Carrot::ECS::EntityID entity, i32 recursionIndex) {
        return Carrot::Identifier { Carrot::sprintf("portal %s %d", entity.toString().c_str(), recursionIndex) };
    }

    static u8 makeStencilValue(i32 portalIndex, i32 recursionIndex) {
        return (portalIndex+1) * PortalComponent::MaxRecursion + recursionIndex;
    }

    void PortalRenderSystem::beginFrame(const Carrot::Render::Context& mainRenderContext) {
        if (regenerateViewports) {
            regenerateViewports = false;
            doViewportRegeneration();
        }
    }

    void PortalRenderSystem::doViewportRegeneration() {
        portalViewportInfo.clear();

        // create viewports for portal
        for (i32 portalIndex = 0; portalIndex < entities.size(); portalIndex++) {
            Entity& entity = entities[portalIndex];
            auto entityID = entity.getID();

            entity.getComponent<ModelComponent>()->hiddenInViewports.clear();

            Carrot::Identifier previousViewportID = MainViewportID;
            for (i32 recursionIndex = 0; recursionIndex < PortalComponent::MaxRecursion; recursionIndex++) {
                Carrot::Identifier id = makeViewportIDFromEntryPortal(entityID, recursionIndex);
                Render::Viewport& viewport = GetEngine().getOrCreateViewport(id);

                Render::StencilSettings viewportStencil{};
                viewportStencil.stencilEnabled = true;
                viewportStencil.stencilCompare = true;
                viewportStencil.stencilValue = makeStencilValue(portalIndex, recursionIndex);
                viewportStencil.stencilOperation = Render::StencilOperation::Equal;
                viewport.setStencilSettings(viewportStencil);
                viewport.enabled = recursionIndex == 0;

                ViewportInfo info {
                    .config = viewportStencil,
                    .entryPortal = entity,
                    .recursionIndex = recursionIndex,
                    .previousViewportID = previousViewportID,
                };

                previousViewportID = id;

                portalViewportInfo[id] = info;
            }
        }

        // TODO: use forEachEntity
        for (i32 portalIndex = 0; portalIndex < entities.size(); portalIndex++) {
            Entity& entity = entities[portalIndex];
            auto entityID = entity.getID();
            Carrot::Identifier id = makeViewportIDFromEntryPortal(entityID, 0);
            Render::Viewport& viewport = GetEngine().getOrCreateViewport(id);

            // make exit portal invisible in viewport rendering the view from out of exit portal
            PortalComponent& portalComp = entity.getComponent<PortalComponent>();
            if (portalComp.otherPortal.exists()) {
                Carrot::Vector<Carrot::Identifier>& hiddenInViewports = portalComp.otherPortal.getComponent<ModelComponent>()->hiddenInViewports;
                for (i32 recursionIndex = 0; recursionIndex < PortalComponent::MaxRecursion; recursionIndex++) {
                    hiddenInViewports.pushBack(makeViewportIDFromEntryPortal(entityID, recursionIndex));
                }
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

                Render::StencilSettings meshStencil;
                meshStencil.stencilEnabled = true;
                meshStencil.stencilWrite = true;
                meshStencil.stencilValuePerViewport.emplace();
                meshStencil.stencilOperationPerViewport.emplace();
                meshStencil.incrementPerViewport.emplace();
                meshStencil.stencilComparePerViewport.emplace();

                // in main viewport, always render portal with a fixed value
                (*meshStencil.stencilValuePerViewport)[MainViewportID] = makeStencilValue(portalIndex, 0);
                (*meshStencil.stencilOperationPerViewport)[MainViewportID] = Render::StencilOperation::AlwaysPass;
                (*meshStencil.incrementPerViewport)[MainViewportID] = false;
                (*meshStencil.stencilComparePerViewport)[MainViewportID] = false;

                // in portal viewports, render only if value matches with "viewport stencil", and increment once (to setup for recursive rendering)
                for (i32 otherPortalIndex = 0; otherPortalIndex < entities.size(); otherPortalIndex++) {
                    for (i32 recursionIndex = 0; recursionIndex < PortalComponent::MaxRecursion; recursionIndex++) {
                        const Carrot::Identifier otherViewportID = makeViewportIDFromEntryPortal(entities[otherPortalIndex], recursionIndex);
                        (*meshStencil.stencilValuePerViewport)[otherViewportID] = portalViewportInfo[otherViewportID].config.stencilValue;
                        (*meshStencil.stencilOperationPerViewport)[otherViewportID] = Render::StencilOperation::Equal;
                        (*meshStencil.incrementPerViewport)[otherViewportID] = true;
                        (*meshStencil.stencilComparePerViewport)[otherViewportID] = true;
                    }
                }

                for(std::size_t i = 0; i < staticMeshCount; i++) {
                    if(Carrot::Render::MaterialOverride* pExistingOverride = modelComp.modelRenderer->getOverrides().findForMesh(i)) {
                        pExistingOverride->stencilSettings = meshStencil;
                    } else {
                        Carrot::Render::MaterialOverride override;
                        override.meshIndex = i;
                        override.stencilSettings = meshStencil;
                        modelComp.modelRenderer->addOverride(override);
                    }
                }
                modelComp.modelRenderer->recreateStructures();
                world.getWorldData().storeModelRenderer(modelComp.modelRenderer);
            }
        }

        Render::ViewportComposition composition;
        Render::ViewportLocation& main = composition.viewports[MainViewportID];
        main.stencil.emplace();
        main.stencil->stencilEnabled = true;
        main.stencil->stencilWrite = true;
        main.stencil->stencilValue = 0;
        main.renderingOrder = 0;
        main.z = 1.0f;

        for (const auto& [id, info] : portalViewportInfo) {
            Render::ViewportLocation& portalView = composition.viewports[id];
            portalView.stencil = info.config;
            portalView.inheritDepthStencil = info.previousViewportID;
            portalView.z = 1.0f / static_cast<float>(info.recursionIndex+1);
            portalView.renderingOrder = info.config.stencilValue; // reuse it as it is already increasing
            portalView.discardIfDepthNotWritten = true;
            portalView.renderingCondition = [id] {
                Memory::OptionalRef<Render::Viewport> viewportRef = GetEngine().getViewport(id);
                if (viewportRef.hasValue()) {
                    return viewportRef->enabled;
                }
                return false;
            };
        }

        GetEngine().getGame()->updateViewportComposition(std::move(composition));
    }

    void PortalRenderSystem::reload() {
    }

    void PortalRenderSystem::unload() {
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

        // Needs to be done in setupCamera instead of beginFrame because portals need position of main camera to render the correct view
        // without this, the view inside portal lag behind
        if (renderContext.pViewport->getViewportID() == MainViewportID) {
            // check if viewports should still be enabled and compute camera positions
            const Render::Viewport& mainViewport = GetEngine().getOrCreateViewport(Carrot::Identifier{"main"});
            const Camera& cameraFromMainViewport = mainViewport.getCamera();
            for (i32 portalIndex = 0; portalIndex < entities.size(); portalIndex++) {
                Entity& entryPortal = entities[portalIndex];
                PortalComponent& portalComp = entryPortal.getComponent<PortalComponent>();
                Entity& exitPortal = portalComp.otherPortal;

                if (exitPortal.exists()) {
                    glm::mat4 cameraStartPosition = glm::inverse(cameraFromMainViewport.getCurrentFrameViewMatrix());
                    const TransformComponent& entryTransform = entryPortal.getComponent<TransformComponent>();
                    const TransformComponent& exitTransform = exitPortal.getComponent<TransformComponent>();
                    const glm::vec3 exitPos = exitTransform.computeFinalPosition();
                    const glm::vec3 exitForward = exitTransform.computeGlobalForward();

                    const glm::vec3 exitRight = glm::cross(exitForward, exitTransform.computeGlobalUp()); // X is forward with current setup

                    auto computeOneLookThroughPortal = [&](const glm::mat4& cameraPosition) -> PortalComponent::CameraData {
                        glm::mat4 finalTransformAsMatrix = exitTransform.toTransformMatrix() * glm::inverse(entryTransform.toTransformMatrix()) * cameraPosition;

                        Math::Transform finalTransform;
                        finalTransform.position = finalTransformAsMatrix[3].xyz();
                        finalTransform.rotation = glm::quat(glm::mat3{finalTransformAsMatrix});

                        Math::Plane clipPlane{};
                        float portalSide = glm::sign(glm::dot(exitPos - finalTransform.position, exitRight));
                        clipPlane.normal = exitRight * portalSide;
                        clipPlane.distanceFromOrigin = -glm::dot(clipPlane.normal, exitPos);

                        return PortalComponent::CameraData {
                            .finalCameraTransform = finalTransformAsMatrix,
                            .worldSpaceClipPlane = clipPlane,
                        };
                    };

                    portalComp.cameraData[0] = computeOneLookThroughPortal(cameraStartPosition);
                    if (auto viewportRef = GetEngine().getViewport(makeViewportIDFromEntryPortal(entryPortal, 0)); viewportRef.hasValue()) {
                        viewportRef->enabled = true;
                    }

                    for (i32 recursionIndex = 1; recursionIndex < PortalComponent::MaxRecursion; recursionIndex++) {
                        const glm::mat4& previousCameraPosition = portalComp.cameraData[recursionIndex-1].finalCameraTransform;
                        portalComp.cameraData[recursionIndex] = computeOneLookThroughPortal(previousCameraPosition);

                        if (auto viewportRef = GetEngine().getViewport(makeViewportIDFromEntryPortal(entryPortal, recursionIndex)); viewportRef.hasValue()) {
                            // TODO: check if overlap between entry & exit
                            viewportRef->enabled = true;
                        }
                    }
                } else {
                    for (i32 recursionIndex = 0; recursionIndex < PortalComponent::MaxRecursion; recursionIndex++) {
                        if (auto viewportRef = GetEngine().getViewport(makeViewportIDFromEntryPortal(entryPortal, recursionIndex)); viewportRef.hasValue()) {
                            viewportRef->enabled = false;
                        }
                    }
                }
            }
        }

        Camera& viewportCamera = renderContext.getCamera();

        auto entryIter = portalViewportInfo.find(renderContext.pViewport->getViewportID());
        if (entryIter == portalViewportInfo.end()) {
            return;
        }

        ViewportInfo& viewportInfo = entryIter->second;

        Carrot::ECS::Entity& entryPortal = viewportInfo.entryPortal;
        if (!entryPortal.exists())
            return;

        PortalComponent& entryPortalComponent = entryPortal.getComponent<PortalComponent>();
        Carrot::ECS::Entity exitPortal = entryPortalComponent.otherPortal;
        if (!exitPortal.exists())
            return;

        const Render::Viewport& mainViewport = GetEngine().getOrCreateViewport(Carrot::Identifier{"main"});
        const Camera& cameraFromMainViewport = mainViewport.getCamera();

        glm::mat4 view = glm::inverse(entryPortalComponent.cameraData[viewportInfo.recursionIndex].finalCameraTransform);
        glm::mat4 projection = cameraFromMainViewport.getProjectionMatrix();
        viewportCamera.setViewProjection(view, projection);
        viewportCamera.setWorldSpaceClippingPlane(entryPortalComponent.cameraData[viewportInfo.recursionIndex].worldSpaceClipPlane);
    }

    std::unique_ptr<Carrot::ECS::System> PortalRenderSystem::duplicate(Carrot::ECS::World& newOwner) const {
        return std::make_unique<PortalRenderSystem>(newOwner);
    }
}
