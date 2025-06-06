//
// Created by jglrxavpok on 28/12/2020.
//

#pragma once
#include "engine/vulkan/SwapchainAware.h"
#include "engine/render/RenderGraph.h"

// TODO: Delete this class and move methods to renderer
namespace Carrot {
    class RayTracer;

    class GBuffer: public SwapchainAware {
    public:
        explicit GBuffer(Carrot::VulkanRenderer& renderer, Carrot::RayTracer& raytracer);

        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(Window& window, int newWidth, int newHeight) override;

    public: // Render::Graph reimpl
        Render::Pass<Carrot::Render::PassData::GBuffer>& addGBufferPass(Render::GraphBuilder& graph, std::function<void(const Carrot::Render::CompiledPass& pass, const Render::Context&, vk::CommandBuffer&)> opaqueCallback, const Render::TextureSize& framebufferSize = {});

    private:
        VulkanRenderer& renderer;
        RayTracer& raytracer;
    };
}
