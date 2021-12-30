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
    void ModelRenderSystem::transparentGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {

    }

    void ModelRenderSystem::opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {

    }

    void ModelRenderSystem::renderModels(const Carrot::Render::Context& renderContext, bool isTransparent) {
/*        for(const auto& [model, transforms] : isTransparent ? transparentEntities : opaqueEntities) {
            auto& instanceBufferMap = isTransparent ? transparentInstancingBuffers : opaqueInstancingBuffers;
            auto& instanceBuffer = instanceBufferMap[model].second;
            model->draw(renderPass, renderContext, commands, *instanceBuffer, transforms.size());
        }*/
        forEachEntity([&](Entity& entity, Transform& transform, ModelComponent& modelComp) {
            if (modelComp.model && modelComp.isTransparent == isTransparent) {
                Carrot::InstanceData instanceData;
                instanceData.transform = transform.toTransformMatrix();
                instanceData.uuid = entity.getID();
                Render::PassEnum pass = isTransparent ? Render::PassEnum::TransparentGBuffer : Render::PassEnum::OpaqueGBuffer;
                modelComp.model->renderStatic(renderContext, instanceData, pass);
            }
        });
    }

    static void allocateBuffer(std::uint32_t instanceCount, std::unique_ptr<Buffer>& out) {
        auto& engine = Carrot::Engine::getInstance();
        out = std::move(engine.getResourceAllocator().allocateDedicatedBuffer(instanceCount * sizeof(Carrot::InstanceData), vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible));
    }

    void ModelRenderSystem::onFrame(Carrot::Render::Context renderContext) {
        /*opaqueEntities.clear();
        transparentEntities.clear();

        forEachEntity([&](Entity& entity, Transform& transform, ModelComponent& modelComp) {
            if(modelComp.model) {
                auto& entityMap = modelComp.isTransparent ? transparentEntities : opaqueEntities;
                entityMap[modelComp.model.get()].push_back(entity);
            }
        });
        auto prepareInstancingBuffers = [&](std::unordered_map<Carrot::Model*, std::vector<Carrot::ECS::Entity>>& entityMap,
                                            std::unordered_map<Carrot::Model*, std::pair<std::uint32_t, std::unique_ptr<Buffer>>>& buffers) {
            for(auto& [model, entities] : entityMap) {
                auto& pair = buffers[model];
                if(pair.first < entities.size()) {
                    allocateBuffer(entities.size(), pair.second);
                    pair.first = entities.size();
                }

                auto& buffer = *pair.second;
                auto* instanceData = buffer.map<InstanceData>();
                for (int i = 0; i < entities.size(); ++i) {
                    auto& entity = entities[i];
                    Transform& transform = entity.getComponent<Transform>();
                    ModelComponent& comp = entity.getComponent<ModelComponent>();
                    auto& instance = instanceData[i];
                    instance.color = comp.color;
                    instance.uuid = glm::u32vec4 { entity.getID().data0(), entity.getID().data1(), entity.getID().data2(), entity.getID().data3() };
                    instance.transform = transform.toTransformMatrix();
                }
            }
        };

        prepareInstancingBuffers(opaqueEntities, opaqueInstancingBuffers);
        prepareInstancingBuffers(transparentEntities, transparentInstancingBuffers);*/
        renderModels(renderContext, true);
        renderModels(renderContext, false);
    }

    std::unique_ptr<System> ModelRenderSystem::duplicate(World& newOwner) const {
        return std::make_unique<ModelRenderSystem>(newOwner);
    }
}
