//
// Created by jglrxavpok on 28/07/2021.
//

#include "SpriteRenderSystem.h"

namespace Carrot::ECS {
    void SpriteRenderSystem::gBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
        for(auto& ptr : entities) {
            if(auto entity = ptr.lock()) {
                auto* spriteComp = world.getComponent<Carrot::ECS::SpriteComponent>(entity);
                if(spriteComp->sprite) {
                    spriteComp->sprite->soloGBufferRender(renderPass, renderContext, commands);
                }
            }
        }
    }

    void SpriteRenderSystem::onFrame(Carrot::Render::Context renderContext) {
        for(auto& ptr : entities) {
            if (auto entity = ptr.lock()) {
                auto* spriteComp = world.getComponent<Carrot::ECS::SpriteComponent>(entity);
                auto* transform = world.getComponent<Carrot::Transform>(entity);
                if(spriteComp->sprite) {
                    updateSprite(renderContext, *transform, *spriteComp->sprite);
                }
            }
        }
    }

    void SpriteRenderSystem::updateSprite(Carrot::Render::Context renderContext, const Carrot::Transform& transform, Carrot::Render::Sprite& sprite) {
        sprite.parentTransform = transform.toTransformMatrix();
        sprite.onFrame(renderContext);
    }

    void SpriteRenderSystem::tick(double dt) {
        for(auto& ptr : entities) {
            if (auto entity = ptr.lock()) {
                auto* spriteComp = world.getComponent<Carrot::ECS::SpriteComponent>(entity);
                if(spriteComp->sprite) {
                    spriteComp->sprite->tick(dt);
                }
            }
        }
    }
}
