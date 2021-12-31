//
// Created by jglrxavpok on 28/07/2021.
//

#include "SpriteRenderSystem.h"
#include <engine/vulkan/CustomTracyVulkan.h>
#include <engine/render/DrawData.h>

namespace Carrot::ECS {
    void SpriteRenderSystem::transparentGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
        forEachEntity([&](Entity& entity, TransformComponent& transform, SpriteComponent& spriteComp) {
            if(spriteComp.sprite && spriteComp.isTransparent) {
                setupEntityData(entity, *spriteComp.sprite, renderContext, commands);
                spriteComp.sprite->soloGBufferRender(renderPass, renderContext, commands);
            }
        });
    }

    void SpriteRenderSystem::opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
        forEachEntity([&](Entity& entity, TransformComponent& transform, SpriteComponent& spriteComp) {
            if(spriteComp.sprite && !spriteComp.isTransparent) {
                setupEntityData(entity, *spriteComp.sprite, renderContext, commands);
                spriteComp.sprite->soloGBufferRender(renderPass, renderContext, commands);
            }
        });
    }

    void SpriteRenderSystem::setupEntityData(const Entity& entity, const Carrot::Render::Sprite& sprite, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands) {
        DrawData entityData;
        entityData.setUUID(entity.getID());
        renderContext.renderer.pushConstantBlock("drawDataPush", sprite.getRenderingPipeline(), renderContext, vk::ShaderStageFlagBits::eFragment, commands, entityData);
    }

    void SpriteRenderSystem::onFrame(Carrot::Render::Context renderContext) {
        forEachEntity([&](Entity& entity, TransformComponent& transform, SpriteComponent& spriteComp) {
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
