//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/ModelComponent.h>

namespace Carrot::ECS {
    class ModelRenderSystem: public RenderSystem<TransformComponent, Carrot::ECS::ModelComponent> {
    public:
        explicit ModelRenderSystem(World& world): RenderSystem<TransformComponent, ModelComponent>(world) {}

        void onFrame(Carrot::Render::Context renderContext) override;

        void transparentGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override;
        void opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;

    private:
        std::unordered_map<Carrot::Model*, std::vector<Carrot::ECS::Entity>> opaqueEntities;
        std::unordered_map<Carrot::Model*, std::vector<Carrot::ECS::Entity>> transparentEntities;
        std::unordered_map<Carrot::Model*, std::pair<std::uint32_t, std::unique_ptr<Buffer>>> opaqueInstancingBuffers;
        std::unordered_map<Carrot::Model*, std::pair<std::uint32_t, std::unique_ptr<Buffer>>> transparentInstancingBuffers;

        void renderModels(const Carrot::Render::Context& renderContext, bool isTransparent);
    };
}
