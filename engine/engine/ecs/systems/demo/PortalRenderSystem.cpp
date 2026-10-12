//
// Created by jglrxavpok on 23/09/2026.
//

#include "PortalRenderSystem.h"

#include <engine/Engine.h>
#include <engine/ecs/components/ModelComponent.h>
#include <glm/gtc/type_ptr.inl>

namespace Carrot::ECS {
    static bool freeze = false;
    constexpr i32 MaxRecursion = 2; // TODO: bump higher once memory issues are fixed

    void PortalRenderSystem::beginFrame(const Carrot::Render::Context& mainRenderContext) {
        if (!regenerateViewports)
            return;

        Carrot::Identifier mainViewportID {"main"};

        regenerateViewports = false;
        portalViewportInfo.clear();

        auto makeViewportIDFromEntryPortal = [](Carrot::ECS::EntityID entity, i32 recursionIndex) {
            return Carrot::Identifier { Carrot::sprintf("portal %s %d", entity.toString().c_str(), recursionIndex) };
        };

        auto makeStencilValue = [](i32 portalIndex, i32 recursionIndex) {
            return (portalIndex+1) * MaxRecursion + recursionIndex;
        };

        // create viewports for portal
        for (i32 portalIndex = 0; portalIndex < entities.size(); portalIndex++) {
            Entity& entity = entities[portalIndex];
            auto entityID = entity.getID();

            entity.getComponent<ModelComponent>()->hiddenInViewports.clear();

            for (i32 recursionIndex = 0; recursionIndex < MaxRecursion; recursionIndex++) {
                Carrot::Identifier id = makeViewportIDFromEntryPortal(entityID, recursionIndex);
                Render::Viewport& viewport = GetEngine().getOrCreateViewport(id);

                Render::StencilSettings viewportStencil{};
                viewportStencil.stencilEnabled = true;
                viewportStencil.stencilCompare = true;
                viewportStencil.stencilValue = makeStencilValue(portalIndex, recursionIndex);
                viewportStencil.stencilOperation = Render::StencilOperation::Equal;
                viewport.setStencilSettings(viewportStencil);
                viewport.enabled = recursionIndex == 0; // TODO: enable only if overlap

                ViewportInfo info {
                    .config = viewportStencil,
                    .entryPortal = entity,
                };

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
                portalComp.otherPortal.getComponent<ModelComponent>()->hiddenInViewports.pushBack(makeViewportIDFromEntryPortal(entityID, 0));
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
                (*meshStencil.stencilValuePerViewport)[mainViewportID] = makeStencilValue(portalIndex, 0);
                (*meshStencil.stencilOperationPerViewport)[mainViewportID] = Render::StencilOperation::AlwaysPass;
                (*meshStencil.incrementPerViewport)[mainViewportID] = false;
                (*meshStencil.stencilComparePerViewport)[mainViewportID] = false;

                // in portal viewports, render only if value matches with "viewport stencil", and increment once (to setup for recursive rendering)
                for (i32 otherPortalIndex = 0; otherPortalIndex < entities.size(); otherPortalIndex++) {
                    for (i32 recursionIndex = 0; recursionIndex < MaxRecursion; recursionIndex++) {
                        const Carrot::Identifier otherViewportID = makeViewportIDFromEntryPortal(entities[otherPortalIndex], recursionIndex);
                        (*meshStencil.stencilValuePerViewport)[otherViewportID] = portalViewportInfo[otherViewportID].config.stencilValue;
                        (*meshStencil.stencilOperationPerViewport)[otherViewportID] = Render::StencilOperation::Equal;
                        (*meshStencil.incrementPerViewport)[otherViewportID] = true;
                        (*meshStencil.stencilComparePerViewport)[otherViewportID] = true;
                    }
                }

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
