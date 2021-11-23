//
// Created by jglrxavpok on 16/11/2021.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/Transform.h>
#include <engine/ecs/components/LightComponent.h>
#include <engine/render/BillboardRenderer.h>

namespace Peeler::ECS {
    class LightEditorRenderer: public Carrot::ECS::RenderSystem<Carrot::ECS::Transform, Carrot::ECS::LightComponent> {
    public:
        explicit LightEditorRenderer(Carrot::ECS::World& world);

        void opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override;

        void onFrame(Carrot::Render::Context renderContext) override;

        std::unique_ptr<Carrot::ECS::System> duplicate(Carrot::ECS::World& newOwner) const override;

    private:
        Carrot::Render::BillboardRenderer billboardRenderer;
        std::shared_ptr<Carrot::Render::TextureHandle> textureHandleOff;
        std::shared_ptr<Carrot::Render::TextureHandle> textureHandleOn;
    };
}
