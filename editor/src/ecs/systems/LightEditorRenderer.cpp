//
// Created by jglrxavpok on 16/11/2021.
//

#include "LightEditorRenderer.h"
#include "engine/Engine.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/render/MaterialSystem.h"
#include "../../Peeler.h"

namespace Peeler::ECS {

    LightEditorRenderer::LightEditorRenderer(Carrot::ECS::World& world): RenderSystem<Carrot::ECS::TransformComponent, Carrot::ECS::LightComponent>(world) {
        textureHandleOn = GetRenderer().getMaterialSystem().createTextureHandle(GetRenderer().getOrCreateTexture("ui/lightbulb_active.png"));
        textureHandleOff = GetRenderer().getMaterialSystem().createTextureHandle(GetRenderer().getOrCreateTexture("ui/lightbulb_inactive.png"));
    }

    void LightEditorRenderer::opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {

    }

    void LightEditorRenderer::onFrame(Carrot::Render::Context renderContext) {
        if(Peeler::Instance->isCurrentlyPlaying()) {
            return;
        }
        forEachEntity([&](Carrot::ECS::Entity& entity, Carrot::ECS::TransformComponent& transform, Carrot::ECS::LightComponent& lightComponent) {
            auto& lightHandle = lightComponent.lightRef;
            if(!lightHandle)
                return;
            const float scale = 0.5f;

            const auto& textureHandle = (lightHandle->light.flags & Carrot::Render::LightFlags::Enabled) != Carrot::Render::LightFlags::None ? textureHandleOn : textureHandleOff;
            billboardRenderer.render(transform.computeFinalPosition(), scale, *textureHandle, renderContext, lightHandle->light.color, entity.getID());
        });
    }

    std::unique_ptr<Carrot::ECS::System> LightEditorRenderer::duplicate(Carrot::ECS::World& newOwner) const {
        return std::make_unique<LightEditorRenderer>(newOwner);
    }
}