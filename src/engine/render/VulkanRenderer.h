//
// Created by jglrxavpok on 13/03/2021.
//

#pragma once

#include <memory>
#include <map>
#include "engine/vulkan/VulkanDriver.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/render/resources/Image.h"

namespace Carrot {
    class GBuffer;
    class ASBuilder;
    class RayTracer;

    class VulkanRenderer {
    private:
        VulkanDriver& driver;

        // TODO: abstraction over textures
        map<string, shared_ptr<Pipeline>> pipelines{};
        map<string, unique_ptr<Image>> textureImages{};
        map<string, vk::UniqueImageView> textureImageViews{};

        vector<unique_ptr<Image>> uiImages{};
        vector<vk::UniqueImageView> uiImageViews{};

        vector<unique_ptr<Image>> skyboxImages{};
        vector<vk::UniqueImageView> skyboxImageViews{};

        vk::UniqueDescriptorPool imguiDescriptorPool{};

        vk::UniqueRenderPass gRenderPass{};
        vk::UniqueRenderPass imguiRenderPass{};
        vk::UniqueRenderPass skyboxRenderPass{};
        vector<vk::UniqueFramebuffer> imguiFramebuffers{};
        vector<vk::UniqueFramebuffer> skyboxFramebuffers{};
        vector<vk::UniqueFramebuffer> swapchainFramebuffers{};

        unique_ptr<RayTracer> raytracer = nullptr;
        unique_ptr<ASBuilder> asBuilder = nullptr;
        unique_ptr<GBuffer> gBuffer = nullptr;

        void createUIResources();

        /// Create the pipeline responsible for lighting via the gbuffer
        void createGBuffer();

        /// Create the object responsible of raytracing operations and subpasses
        void createRayTracer();

        void initImgui();

        void createSkyboxRenderPass();

    public:
        explicit VulkanRenderer(VulkanDriver& driver);

        shared_ptr<Pipeline> getOrCreatePipeline(const string& name);

        unique_ptr<Image>& getOrCreateTexture(const string& textureName);
        vk::UniqueImageView& getOrCreateTextureView(const string& textureName);

        void recreateDescriptorPools(size_t frameCount);

        /// Create the render pass
        void createRenderPasses();

        /// Create the framebuffers to render to
        void createFramebuffers();

        size_t getSwapchainImageCount() { return driver.getSwapchainImageCount(); };
        VulkanDriver& getVulkanDriver() { return driver; };

        ASBuilder& getASBuilder() { return *asBuilder; };
        RayTracer& getRayTracer() { return *raytracer; };

        GBuffer& getGBuffer() { return *gBuffer; };

        vector<vk::UniqueFramebuffer>& getSwapchainFramebuffers() { return swapchainFramebuffers; };
        vector<vk::UniqueFramebuffer>& getImguiFramebuffers() { return imguiFramebuffers; };
        vector<vk::UniqueFramebuffer>& getSkyboxFramebuffers() { return skyboxFramebuffers; };

        vk::RenderPass& getGRenderPass() { return *gRenderPass; };
        vk::RenderPass& getImguiRenderPass() { return *imguiRenderPass; };
        vk::RenderPass& getSkyboxRenderPass() { return *skyboxRenderPass; };

        vector<unique_ptr<Image>>& getUIImages() { return uiImages; };
        vector<vk::UniqueImageView>& getUIImageViews() { return uiImageViews; };

        vector<unique_ptr<Image>>& getSkyboxImages() { return skyboxImages; };
        vector<vk::UniqueImageView>& getSkyboxImageViews() { return skyboxImageViews; };

        vk::Device& getLogicalDevice() { return driver.getLogicalDevice(); };

        void createSkyboxResources();
    };
}
