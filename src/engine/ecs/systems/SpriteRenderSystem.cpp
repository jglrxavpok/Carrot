//
// Created by jglrxavpok on 28/07/2021.
//

#include "SpriteRenderSystem.h"

namespace Carrot::ECS {
    void SpriteRenderSystem::transparentGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
        forEachEntity([&](Entity& entity, Transform& transform, SpriteComponent& spriteComp) {
            if(spriteComp.sprite && spriteComp.isTransparent) {
                spriteComp.sprite->soloGBufferRender(renderPass, renderContext, commands);
            }
        });
    }

    void SpriteRenderSystem::opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
        forEachEntity([&](Entity& entity, Transform& transform, SpriteComponent& spriteComp) {
            if(spriteComp.sprite && !spriteComp.isTransparent) {
                spriteComp.sprite->soloGBufferRender(renderPass, renderContext, commands);
            }
        });
    }

    void SpriteRenderSystem::onFrame(Carrot::Render::Context renderContext) {
        forEachEntity([&](Entity& entity, Transform& transform, SpriteComponent& spriteComp) {
            if(spriteComp.sprite) {
                updateSprite(renderContext, transform, *spriteComp.sprite);
            }
        });
    }

    void SpriteRenderSystem::updateSprite(Carrot::Render::Context renderContext, const Transform& transform, Carrot::Render::Sprite& sprite) {
        sprite.parentTransform = transform.toTransformMatrix();
        sprite.onFrame(renderContext);
    }

    void SpriteRenderSystem::tick(double dt) {
        forEachEntity([&](Entity& entity, Transform& transform, SpriteComponent& spriteComp) {
            if(spriteComp.sprite) {
                spriteComp.sprite->tick(dt);
            }
        });
    }
}
