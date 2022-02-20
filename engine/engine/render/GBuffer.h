//
// Created by jglrxavpok on 28/12/2020.
//

#pragma once
#include "engine/vulkan/includes.h"
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
        Render::Pass<Carrot::Render::PassData::GBuffer>& addGBufferPass(Render::GraphBuilder& graph, std::function<void(const Carrot::Render::CompiledPass& pass, const Render::Context&, vk::CommandBuffer&)> opaqueCallback);
        Render::Pass<Carrot::Render::PassData::GBufferTransparent>& addTransparentGBufferPass(Render::GraphBuilder& graph, const Carrot::Render::PassData::GBuffer& data, std::function<void(const Carrot::Render::CompiledPass& pass, const Render::Context&, vk::CommandBuffer&)> transparentCallback);
        Render::Pass<Carrot::Render::PassData::GResolve>& addGResolvePass(const Carrot::Render::PassData::GBuffer& opaqueData, const Carrot::Render::PassData::GBufferTransparent& transparentData, const Render::FrameResource& skyboxOutput, Render::GraphBuilder& graph);

    private:
        VulkanRenderer& renderer;
        RayTracer& raytracer;
        Carrot::Render::Texture::Ref blueNoise;
    };
}
