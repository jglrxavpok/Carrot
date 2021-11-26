//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/Transform.h>
#include <engine/ecs/components/SpriteComponent.h>

namespace Carrot::ECS {
    class SpriteRenderSystem: public RenderSystem<Transform, Carrot::ECS::SpriteComponent> {
    public:
        explicit SpriteRenderSystem(World& world): RenderSystem<Transform, SpriteComponent>(world) {}

        void onFrame(Carrot::Render::Context renderContext) override;

        void transparentGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override;
        void opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override;

        void tick(double dt) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;

    private:
        void setupEntityData(const Entity& entity, const Carrot::Render::Sprite& sprite, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands);
        void updateSprite(Carrot::Render::Context renderContext, const Transform& transform, Carrot::Render::Sprite& sprite);
    };
}
