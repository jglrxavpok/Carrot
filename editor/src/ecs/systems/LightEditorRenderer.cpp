//
// Created by jglrxavpok on 16/11/2021.
//

#include "LightEditorRenderer.h"
#include "engine/Engine.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/render/MaterialSystem.h"

namespace Peeler::ECS {

    LightEditorRenderer::LightEditorRenderer(Carrot::ECS::World& world): RenderSystem<Carrot::ECS::Transform, Carrot::ECS::LightComponent>(world) {
        textureHandleOn = GetRenderer().getMaterialSystem().createTextureHandle(GetRenderer().getOrCreateTexture("ui/lightbulb_active.png"));
        textureHandleOff = GetRenderer().getMaterialSystem().createTextureHandle(GetRenderer().getOrCreateTexture("ui/lightbulb_inactive.png"));
    }

    void LightEditorRenderer::opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
        forEachEntity([&](Carrot::ECS::Entity& entity, Carrot::ECS::Transform& transform, Carrot::ECS::LightComponent& lightComponent) {
            auto& lightHandle = lightComponent.lightRef;
            if(!lightHandle)
                return;
            const float scale = 0.5f;

            const auto& textureHandle = lightHandle->light.enabled ? textureHandleOn : textureHandleOff;
            billboardRenderer.gBufferDraw(transform.computeFinalPosition(), scale, *textureHandle, renderPass, renderContext, commands, lightHandle->light.color, entity.getID());
        });
    }

    void LightEditorRenderer::onFrame(Carrot::Render::Context renderContext) {

    }

    std::unique_ptr<Carrot::ECS::System> LightEditorRenderer::duplicate(Carrot::ECS::World& newOwner) const {
        return std::make_unique<LightEditorRenderer>(newOwner);
    }
}