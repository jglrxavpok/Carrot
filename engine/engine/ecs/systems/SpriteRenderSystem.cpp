//
// Created by jglrxavpok on 28/07/2021.
//

#include "SpriteRenderSystem.h"
#include <engine/vulkan/CustomTracyVulkan.h>
#include <engine/render/GBufferDrawData.h>

namespace Carrot::ECS {
    void SpriteRenderSystem::transparentGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
        // TODO: remove
    }

    void SpriteRenderSystem::opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
        // TODO: remove
    }

    void SpriteRenderSystem::setupEntityData(const Entity& entity, const Carrot::Render::Sprite& sprite, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands) {
        // TODO: remove
    }

    void SpriteRenderSystem::onFrame(Carrot::Render::Context renderContext) {
        forEachEntity([&](Entity& entity, TransformComponent& transform, SpriteComponent& spriteComp) {
            if(!entity.isVisible()) {
                return;
            }

            if(spriteComp.sprite) {
                updateSprite(renderContext, transform, *spriteComp.sprite);
            }
        });
    }

    void SpriteRenderSystem::updateSprite(Carrot::Render::Context renderContext, const TransformComponent& transform, Carrot::Render::Sprite& sprite) {
        ZoneScoped;
        sprite.parentTransform = transform.toTransformMatrix();
        sprite.onFrame(renderContext);
    }

    void SpriteRenderSystem::tick(double dt) {
        forEachEntity([&](Entity& entity, TransformComponent& transform, SpriteComponent& spriteComp) {
            if(spriteComp.sprite) {
                spriteComp.sprite->tick(dt);
            }
        });
    }

    std::unique_ptr<System> SpriteRenderSystem::duplicate(World& newOwner) const {
        return std::make_unique<SpriteRenderSystem>(newOwner);
    }
}
