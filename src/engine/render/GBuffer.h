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

namespace Carrot {
    class GBuffer: public SwapchainAware {
    private:
        VulkanRenderer& renderer;
        RayTracer& raytracer;
        vector<unique_ptr<Render::Texture>> viewPositionTextures{};

        vector<unique_ptr<Render::Texture>> albedoTextures{};

        vector<unique_ptr<Render::Texture>> viewNormalTextures{};

        vector<unique_ptr<Render::Texture>> depthStencilTextures{};

        vector<unique_ptr<Render::Texture>> intPropertiesTextures{};

        mutable shared_ptr<Pipeline> gResolvePipeline = nullptr;
        unique_ptr<Mesh> screenQuadMesh = nullptr;

        void generateImages();

    public:
        explicit GBuffer(Carrot::VulkanRenderer& renderer, Carrot::RayTracer& raytracer);

        void loadResolvePipeline();

        void recordResolvePass(uint32_t frameIndex, vk::CommandBuffer& commandBuffer, vk::CommandBufferInheritanceInfo* inheritance) const;

        void addFramebufferAttachments(uint32_t frameIndex, vector<vk::ImageView>& attachments);

        vk::UniqueRenderPass createRenderPass();

        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

        vector<std::unique_ptr<Render::Texture>>& getIntPropertiesTextures() { return intPropertiesTextures; }
        vector<std::unique_ptr<Render::Texture>>& getDepthStencilTextures() { return depthStencilTextures; }
        vector<std::unique_ptr<Render::Texture>>& getNormalTextures() { return viewNormalTextures; }
        vector<std::unique_ptr<Render::Texture>>& getPositionTextures() { return viewPositionTextures; }
        vector<std::unique_ptr<Render::Texture>>& getAlbedoTextures() { return albedoTextures; }

    public: // Render::Graph reimpl
        struct GBufferData {
            Render::FrameResource positions;
            Render::FrameResource normals;
            Render::FrameResource albedo;
            Render::FrameResource depthStencil;
            Render::FrameResource flags;
        };

        struct GResolveData {
            Render::FrameResource positions;
            Render::FrameResource normals;
            Render::FrameResource albedo;
            Render::FrameResource depthStencil;
            Render::FrameResource flags;

            Render::FrameResource resolved;
        };

        Render::Pass<GBufferData>& addGBufferPass(Render::GraphBuilder& graph, std::function<void(const Carrot::Render::CompiledPass& pass, const Render::FrameData&, vk::CommandBuffer&)> callback);
        Render::Pass<GResolveData>& addGResolvePass(const GBufferData& data, Render::GraphBuilder& graph);
    };
}
