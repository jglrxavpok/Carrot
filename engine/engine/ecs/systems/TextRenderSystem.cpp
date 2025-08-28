//
// Created by jglrxavpok on 28/07/2021.
//

#include "TextRenderSystem.h"
#include <engine/vulkan/CustomTracyVulkan.h>
#include <engine/render/GBufferDrawData.h>

namespace Carrot::ECS {

    void TextRenderSystem::onFrame(const Carrot::Render::Context& renderContext) {
        forEachEntity([&](Entity& entity, TransformComponent& transform, TextComponent& textComponent) {
            if(!entity.isVisible()) {
                return;
            }

            textComponent.refreshRenderable();
            textComponent.renderableText.getTransform() = transform.toTransformMatrix();
            textComponent.renderableText.render(renderContext);
        });
    }

    std::unique_ptr<System> TextRenderSystem::duplicate(World& newOwner) const {
        return std::make_unique<TextRenderSystem>(newOwner);
    }
}
