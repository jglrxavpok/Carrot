//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/SpriteComponent.h>

namespace Carrot::ECS {
    class SpriteRenderSystem: public RenderSystem<TransformComponent, Carrot::ECS::SpriteComponent>, public Identifiable<SpriteRenderSystem> {
    public:
        explicit SpriteRenderSystem(World& world): RenderSystem<TransformComponent, SpriteComponent>(world) {}
        explicit SpriteRenderSystem(const rapidjson::Value& json, World& world): SpriteRenderSystem(world) {}

        void onFrame(Carrot::Render::Context renderContext) override;

        void transparentGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override;
        void opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override;

        void tick(double dt) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;

    public:
        inline static const char* getStringRepresentation() {
            return "SpriteRender";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }

    private:
        void setupEntityData(const Entity& entity, const Carrot::Render::Sprite& sprite, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands);
        void updateSprite(Carrot::Render::Context renderContext, const TransformComponent& transform, Carrot::Render::Sprite& sprite);
    };
}
