//
// Created by jglrxavpok on 28/07/2021.
//

#include "ModelRenderSystem.h"
#include <engine/vulkan/CustomTracyVulkan.h>
#include <engine/render/GBufferDrawData.h>
#include <engine/render/resources/ResourceAllocator.h>
#include <engine/render/InstanceData.h>
#include <engine/render/RenderPacket.h>
#include <engine/render/ClusterManager.h>

namespace Carrot::ECS {
    ModelRenderSystem::ModelRenderSystem(const rapidjson::Value& json, World& world): RenderSystem<TransformComponent, ModelComponent>(world) {

    }

    void ModelRenderSystem::transparentGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {

    }

    void ModelRenderSystem::opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {

    }

    void ModelRenderSystem::renderModels(const Carrot::Render::Context& renderContext) {
        parallelForEachEntity([&](Entity& entity, TransformComponent& transform, ModelComponent& modelComp) {
            ZoneScopedN("Per entity");
            if(!entity.isVisible()) {
                auto pMeshlets = modelComp.rendererStorage.clustersInstancePerViewport[renderContext.pViewport];
                if(pMeshlets) {
                    pMeshlets->enabled = false;
                }
                return;
            }

            if (modelComp.asyncModel.isReady()) {
                Carrot::InstanceData instanceData;
                instanceData.lastFrameTransform = transform.lastFrameGlobalTransform;
                instanceData.transform = transform.toTransformMatrix();
                instanceData.uuid = entity.getID();
                instanceData.color = modelComp.color;

                if(modelComp.modelRenderer) {
                    modelComp.modelRenderer->render(modelComp.rendererStorage, renderContext, instanceData, Render::PassEnum::OpaqueGBuffer);
                } else {
                    // TODO: support for virtualized geometry?
                    modelComp.asyncModel->renderStatic(modelComp.rendererStorage, renderContext, instanceData, Render::PassEnum::OpaqueGBuffer);
                }
                //modelComp.asyncModel->renderStatic(renderContext, instanceData, Render::PassEnum::TransparentGBuffer);

                modelComp.loadTLASIfPossible();
                if(modelComp.tlas) {
                    modelComp.tlas->transform = instanceData.transform;
                    modelComp.tlas->customIndex = 0; // TODO?
                    modelComp.tlas->instanceColor = modelComp.color;

                    if(modelComp.tlas->enabled != modelComp.castsShadows) { // avoid locking for a simple check
                        if(modelComp.castsShadows) {
                            modelComp.enableTLAS();
                        } else {
                            modelComp.disableTLAS();
                        }
                    }
                }
            }
        });

    }

    void ModelRenderSystem::onFrame(Carrot::Render::Context renderContext) {
        renderModels(renderContext);
    }

    std::unique_ptr<System> ModelRenderSystem::duplicate(World& newOwner) const {
        return std::make_unique<ModelRenderSystem>(newOwner);
    }

    void ModelRenderSystem::reload() {
        forEachEntity([&](Entity& entity, TransformComponent& transform, ModelComponent& modelComp) {
            modelComp.enableTLAS();
        });
    }

    void ModelRenderSystem::unload() {
        forEachEntity([&](Entity& entity, TransformComponent& transform, ModelComponent& modelComp) {
            modelComp.disableTLAS();
        });
    }
}
