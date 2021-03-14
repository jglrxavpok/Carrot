//
// Created by jglrxavpok on 28/12/2020.
//

#pragma once
#include "engine/vulkan/includes.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/render/resources/Mesh.h"
#include "engine/render/raytracing/RayTracer.h"

namespace Carrot {
    class GBuffer {
    private:
        VulkanRenderer& renderer;
        RayTracer& raytracer;
        vector<unique_ptr<Image>> viewPositionImages{};
        vector<vk::UniqueImageView> viewPositionImageViews{};

        vector<unique_ptr<Image>> albedoImages{};
        vector<vk::UniqueImageView> albedoImageViews{};

        vector<unique_ptr<Image>> viewNormalImages{};
        vector<vk::UniqueImageView> viewNormalImageViews{};

        vector<unique_ptr<Image>> depthImages{};
        vector<vk::UniqueImageView> depthStencilImageViews{};
        vector<vk::UniqueImageView> depthOnlyImageViews{};

        mutable shared_ptr<Pipeline> gResolvePipeline = nullptr;
        unique_ptr<Mesh> screenQuadMesh = nullptr;

    public:
        explicit GBuffer(Carrot::VulkanRenderer& renderer, Carrot::RayTracer& raytracer);

        void loadResolvePipeline();

        void recordResolvePass(uint32_t frameIndex, vk::CommandBuffer& commandBuffer, vk::CommandBufferInheritanceInfo* inheritance) const;
        void onSwapchainRecreation();


        void addFramebufferAttachments(uint32_t frameIndex, vector<vk::ImageView>& attachments);

        vk::UniqueRenderPass createRenderPass();
    };
}
