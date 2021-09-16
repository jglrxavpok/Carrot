//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once

#include "engine/Engine.h"
#include "IDTypes.h"
#include "engine/vulkan/SwapchainAware.h"

namespace Carrot {
    class Material: public SwapchainAware {
    private:
        MaterialID id = 0;
        Carrot::Engine& engine;
        std::shared_ptr<Pipeline> pipeline = nullptr;
        std::shared_ptr<Pipeline> renderingPipeline = nullptr;
        TextureID textureID{0xFFFFFFFF};
        bool ignoreInstanceColor = false;

    public:
        explicit Material(Carrot::Engine& engine, const std::string& materialName);

        MaterialID getMaterialID() const;
        TextureID getTextureID() const;
        void setTextureID(TextureID texID);

        bool ignoresInstanceColor() const;

        void bindForRender(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) const;

        const Pipeline& getPipeline() const;
        const Pipeline& getRenderingPipeline() const;
    };
}
