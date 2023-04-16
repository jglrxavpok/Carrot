//
// Created by jglrxavpok on 28/12/2020.
//

#pragma once
#include "engine/vulkan/SwapchainAware.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/render/resources/Mesh.h"
#include "engine/render/raytracing/RayTracer.h"
#include "engine/render/resources/Texture.h"
#include "engine/render/RenderGraph.h"

// TODO: Delete this class and move methods to renderer
namespace Carrot {
    class GBuffer: public SwapchainAware {
    public:
        explicit GBuffer(Carrot::VulkanRenderer& renderer, Carrot::RayTracer& raytracer);

        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    public: // Render::Graph reimpl
        Render::Pass<Carrot::Render::PassData::GBuffer>& addGBufferPass(Render::GraphBuilder& graph, std::function<void(const Carrot::Render::CompiledPass& pass, const Render::Context&, vk::CommandBuffer&)> opaqueCallback, const Render::TextureSize& framebufferSize = {});
        Render::Pass<Carrot::Render::PassData::Lighting>& addLightingPass(const Carrot::Render::PassData::GBuffer& opaqueData, Render::GraphBuilder& graph, const Render::TextureSize& framebufferSize = {});

    public:
        Carrot::Render::Texture::Ref getBlueNoiseTexture() { return blueNoise; }

    private:
        VulkanRenderer& renderer;
        RayTracer& raytracer;
        Carrot::Render::Texture::Ref blueNoise;
    };
}
