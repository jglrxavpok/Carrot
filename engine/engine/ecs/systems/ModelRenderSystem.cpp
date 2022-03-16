//
// Created by jglrxavpok on 28/07/2021.
//

#include "ModelRenderSystem.h"
#include <engine/vulkan/CustomTracyVulkan.h>
#include <engine/render/DrawData.h>
#include <engine/render/resources/ResourceAllocator.h>
#include <engine/render/InstanceData.h>
#include <engine/render/RenderPacket.h>

namespace Carrot::ECS {
    ModelRenderSystem::ModelRenderSystem(const rapidjson::Value& json, World& world): RenderSystem<TransformComponent, ModelComponent>(world) {

    }

    void ModelRenderSystem::transparentGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {

    }

    void ModelRenderSystem::opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {

    }

    void ModelRenderSystem::renderModels(const Carrot::Render::Context& renderContext, bool isTransparent) {
        parallelForEachEntity([&](Entity& entity, TransformComponent& transform, ModelComponent& modelComp) {
            ZoneScopedN("Per entity");
            if(modelComp.isTransparent == isTransparent) {
                if (modelComp.asyncModel.isReady()) {
                    modelComp.loadTLASIfPossible();
                    Carrot::InstanceData instanceData;
                    instanceData.transform = transform.toTransformMatrix();
                    instanceData.uuid = entity.getID();
                    instanceData.color = modelComp.color;
                    Render::PassEnum pass = isTransparent ? Render::PassEnum::TransparentGBuffer : Render::PassEnum::OpaqueGBuffer;

                    modelComp.asyncModel->renderStatic(renderContext, instanceData, pass);

                    if(modelComp.tlas) {
                        modelComp.tlas->transform = instanceData.transform;
                        modelComp.tlas->customIndex = 0; // TODO
                    }
                }
            }
        });
    }

    static void allocateBuffer(std::uint32_t instanceCount, std::unique_ptr<Buffer>& out) {
        auto& engine = Carrot::Engine::getInstance();
        out = std::move(engine.getResourceAllocator().allocateDedicatedBuffer(instanceCount * sizeof(Carrot::InstanceData), vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible));
    }

    void ModelRenderSystem::onFrame(Carrot::Render::Context renderContext) {
        renderModels(renderContext, true);
        renderModels(renderContext, false);
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
