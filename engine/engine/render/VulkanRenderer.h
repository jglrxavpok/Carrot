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
#include "engine/render/MaterialSystem.h"
#include "engine/render/lighting/Lights.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

namespace sol {
    class state;
}

namespace Carrot {
    struct BindingKey {
        vk::PipelineLayout pipeline;
        std::size_t swapchainIndex;
        std::uint32_t setID;
        std::uint32_t bindingID;

        bool operator==(const BindingKey& other) const {
            return pipeline == other.pipeline && swapchainIndex == other.swapchainIndex && setID == other.setID && bindingID == other.bindingID;
        }
    };
};

namespace std {
    template<>
    struct hash<Carrot::BindingKey> {
        std::size_t operator()(const Carrot::BindingKey& key) const {
            const std::size_t prime = 31;

            std::size_t hash = static_cast<std::size_t>((std::uint64_t)key.pipeline.operator VkPipelineLayout_T *());
            hash = key.swapchainIndex + hash * prime;
            hash = key.setID + hash * prime;
            hash = key.bindingID + hash * prime;
            return hash;
        }
    };
}

namespace Carrot {
    class GBuffer;
    class ASBuilder;
    class RayTracer;
    class Mesh;
    class Engine;
    class BufferView;
    class Model;

    using CommandBufferConsumer = std::function<void(vk::CommandBuffer&)>;

    class VulkanRenderer: public SwapchainAware {
    public:
        static constexpr std::uint32_t DefaultCameraDescriptorSetID = 2;
        static constexpr std::uint32_t MaxCameras = 10; // used to determine descriptor set pool size

        explicit VulkanRenderer(VulkanDriver& driver, Configuration config);

        /// Gets or creates the pipeline with the given name (see resources/pipelines).
        /// Instance offset can be used to force the engine to create a new instance. (Can be used for different blit pipelines, each with a different texture)
        std::shared_ptr<Pipeline> getOrCreatePipeline(const std::string& name, std::uint64_t instanceOffset = 0);

        /// Gets or creates the pipeline with the given name (see resources/pipelines).
        /// Different render passes can be used to force the engine to create a new instance. (Can be used for different blit pipelines, each with a different texture)
        std::shared_ptr<Pipeline> getOrCreateRenderPassSpecificPipeline(const std::string& name, const vk::RenderPass& pass);

        std::shared_ptr<Render::Texture> getOrCreateTexture(const std::string& textureName);
        std::shared_ptr<Render::Texture> getOrCreateTextureFullPath(const std::string& textureName);
        std::shared_ptr<Render::Texture> getOrCreateTextureFromResource(const Carrot::IO::Resource& from);

        std::shared_ptr<Model> getOrCreateModel(const std::string& modelPath);

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

        Render::MaterialSystem& getMaterialSystem() { return materialSystem; }

        Render::Lighting& getLighting() { return lighting; }

    public:
        void onSwapchainSizeChange(int newWidth, int newHeight) override;

        void onSwapchainImageCountChange(std::size_t newCount) override;

        void onFrame(const Carrot::Render::Context& renderContext);

    public:
        void initImGuiPass(const vk::RenderPass& renderPass);
        Render::Pass<Carrot::Render::PassData::ImGui>& addImGuiPass(Render::GraphBuilder& graph);

    public:
        const vk::DescriptorSetLayout& getCameraDescriptorSetLayout() const;

        std::vector<vk::DescriptorSet> createDescriptorSetForCamera(const std::vector<Carrot::BufferView>& uniformBuffers);
        void destroyCameraDescriptorSets(const std::vector<vk::DescriptorSet>& sets);

    public:
        void bindSampler(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, const vk::Sampler& samplerToBind, std::uint32_t setID, std::uint32_t bindingID);
        void bindTexture(Pipeline& pipeline, const Render::Context& data, const Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor, vk::ImageViewType viewType = vk::ImageViewType::e2D, std::uint32_t arrayIndex = 0);
        void bindTexture(Pipeline& pipeline, const Render::Context& data, const Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::Sampler sampler, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor, vk::ImageViewType viewType = vk::ImageViewType::e2D, std::uint32_t arrayIndex = 0);

        template<typename ConstantBlock>
        void pushConstantBlock(std::string_view pushName, const Carrot::Pipeline& pipeline, const Carrot::Render::Context& context, vk::ShaderStageFlags stageFlags, vk::CommandBuffer& cmds, const ConstantBlock& block);

        void bindCameraSet(vk::PipelineBindPoint bindPoint, const vk::PipelineLayout& pipelineLayout, const Render::Context& data, vk::CommandBuffer& cmds, std::uint32_t setID = DefaultCameraDescriptorSetID);

        void blit(Carrot::Render::Texture& source, Carrot::Render::Texture& destination, vk::CommandBuffer& cmds, vk::Offset3D srcOffset = {}, vk::Offset3D dstOffset = {});

        void fullscreenBlit(const vk::RenderPass& pass, const Carrot::Render::Context& frame, Carrot::Render::Texture& textureToBlit, Carrot::Render::Texture& targetTexture, vk::CommandBuffer& cmds);

    public:
        Render::Texture::Ref getDefaultImage();

    public:
        static void registerUsertype(sol::state& destination);

    private:
        VulkanDriver& driver;
        Configuration config;

        vk::UniqueDescriptorSetLayout cameraDescriptorSetLayout{};
        vk::UniqueDescriptorPool cameraDescriptorPool{};

        std::map<std::pair<std::string, std::uint64_t>, std::shared_ptr<Pipeline>> pipelines{};
        std::map<std::string, Render::Texture::Ref> textures{};
        Render::MaterialSystem materialSystem;
        Render::Lighting lighting;
        std::map<std::string, std::shared_ptr<Model>> models{};

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

        std::unordered_map<BindingKey, vk::Image> boundTextures;
        std::unordered_map<BindingKey, vk::Sampler> boundSamplers;

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
