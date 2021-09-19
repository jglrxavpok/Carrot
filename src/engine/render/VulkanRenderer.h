//
// Created by jglrxavpok on 13/03/2021.
//

#pragma once

#include <memory>
#include <map>
#include <functional>
#include "engine/vulkan/VulkanDriver.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/render/resources/Texture.h"
#include "engine/vulkan/SwapchainAware.h"
#include "engine/render/RenderContext.h"
#include "engine/render/RenderGraph.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

namespace sol {
    class state;
}

namespace Carrot {
    class GBuffer;
    class ASBuilder;
    class RayTracer;
    class Mesh;
    class Engine;
    class BufferView;

    using CommandBufferConsumer = std::function<void(vk::CommandBuffer&)>;

    class VulkanRenderer: public SwapchainAware {
    public:
        static constexpr std::uint32_t DefaultCameraDescriptorSetID = 2;

        explicit VulkanRenderer(VulkanDriver& driver, Configuration config);

        /// Gets or creates the pipeline with the given name (see resources/pipelines).
        /// Instance offset can be used to force the engine to create a new instance. (Can be used for different blit pipelines, each with a different texture)
        std::shared_ptr<Pipeline> getOrCreatePipeline(const std::string& name, std::uint64_t instanceOffset = 0);

        /// Gets or creates the pipeline with the given name (see resources/pipelines).
        /// Different render passes can be used to force the engine to create a new instance. (Can be used for different blit pipelines, each with a different texture)
        std::shared_ptr<Pipeline> getOrCreateRenderPassSpecificPipeline(const std::string& name, const vk::RenderPass& pass);

        std::shared_ptr<Render::Texture>& getOrCreateTexture(const std::string& textureName);

        void recreateDescriptorPools(std::size_t frameCount);

    public:
        void beforeFrameCommand(const CommandBufferConsumer& command);
        void afterFrameCommand(const CommandBufferConsumer& command);

        /// Call at start of frame (sets ups ImGui stuff)
        void newFrame();
        void preFrame();
        void postFrame();

    public:
        std::size_t getSwapchainImageCount() { return driver.getSwapchainImageCount(); };
        VulkanDriver& getVulkanDriver() { return driver; };

        ASBuilder& getASBuilder() { return *asBuilder; };
        RayTracer& getRayTracer() { return *raytracer; };

        GBuffer& getGBuffer() { return *gBuffer; };

        vk::Device& getLogicalDevice() { return driver.getLogicalDevice(); };

        Mesh& getFullscreenQuad() { return *fullscreenQuad; }

        const Configuration& getConfiguration() { return config; }

        Engine& getEngine();

    public:
        void onSwapchainSizeChange(int newWidth, int newHeight) override;

        void onSwapchainImageCountChange(std::size_t newCount) override;

    public:
        void initImGuiPass(const vk::RenderPass& renderPass);
        Render::Pass<Carrot::Render::PassData::ImGui>& addImGuiPass(Render::GraphBuilder& graph);

    public:
        std::vector<vk::DescriptorSet> createDescriptorSetForCamera(const std::vector<Carrot::BufferView>& uniformBuffers);

    public:
        void bindSampler(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, const vk::Sampler& samplerToBind, std::uint32_t setID, std::uint32_t bindingID);
        void bindTexture(Pipeline& pipeline, const Render::Context& data, const Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor, vk::ImageViewType viewType = vk::ImageViewType::e2D, std::uint32_t arrayIndex = 0);
        void bindTexture(Pipeline& pipeline, const Render::Context& data, const Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::Sampler sampler, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor, vk::ImageViewType viewType = vk::ImageViewType::e2D, std::uint32_t arrayIndex = 0);

        template<typename ConstantBlock>
        void pushConstantBlock(Carrot::Pipeline& pipeline, const Carrot::Render::Context& context, vk::ShaderStageFlags stageFlags, vk::CommandBuffer& cmds, const ConstantBlock& block);

        template<typename ConstantBlock>
        void pushConstantBlock(const vk::PipelineLayout& pipeline, const Carrot::Render::Context& context, vk::ShaderStageFlags stageFlags, vk::CommandBuffer& cmds, const ConstantBlock& block);

        void bindMainCameraSet(vk::PipelineBindPoint bindPoint, const vk::PipelineLayout& pipelineLayout, const Render::Context& data, vk::CommandBuffer& cmds, std::uint32_t setID = DefaultCameraDescriptorSetID);

        void blit(Carrot::Render::Texture& source, Carrot::Render::Texture& destination, vk::CommandBuffer& cmds, vk::Offset3D srcOffset = {}, vk::Offset3D dstOffset = {});

    public:
        static void registerUsertype(sol::state& destination);

    private:
        VulkanDriver& driver;
        Configuration config;

        vk::UniqueDescriptorSetLayout cameraDescriptorSetLayout{};
        vk::UniqueDescriptorPool cameraDescriptorPool{};

        std::map<std::pair<std::string, std::uint64_t>, std::shared_ptr<Pipeline>> pipelines{};
        std::map<std::string, Render::Texture::Ref> textures{};

        vk::UniqueDescriptorPool imguiDescriptorPool{};

        std::unique_ptr<RayTracer> raytracer = nullptr;
        std::unique_ptr<ASBuilder> asBuilder = nullptr;
        std::unique_ptr<GBuffer> gBuffer = nullptr;

        std::list<CommandBufferConsumer> beforeFrameCommands;
        std::list<CommandBufferConsumer> afterFrameCommands;

        ImGui_ImplVulkan_InitInfo imguiInitInfo;
        bool imguiIsInitialized = false;
        std::unique_ptr<Carrot::Mesh> fullscreenQuad = nullptr;
        std::list<std::unique_ptr<std::uint8_t[]>> pushConstants;

        void createCameraSetResources();

        void createUIResources();

        /// Create the pipeline responsible for lighting via the gbuffer
        void createGBuffer();

        /// Create the object responsible of raytracing operations and subpasses
        void createRayTracer();

        void initImGui();
    };
}

#include "VulkanRenderer.ipp"
