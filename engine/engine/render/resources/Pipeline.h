//
// Created by jglrxavpok on 01/12/2020.
//

#pragma once
#include "engine/vulkan/VulkanDriver.h"
#include "engine/vulkan/SwapchainAware.h"
#include "engine/render/IDTypes.h"
#include "engine/render/shaders/ShaderStages.h"
#include "VertexFormat.h"
#include "engine/render/shaders/ShaderSource.h"
#include <core/utils/Lookup.hpp>

namespace Carrot {
    class Material;

    class Buffer;

    // TODO: needs to be removed and replaced by data-oriented design
    enum class PipelineType {
        GBuffer,
        UnlitGBuffer,
        Lighting,
        Skybox,
        Blit,
        Particles,
        Denoising,
        Compute,
        Unknown
    };

    struct PipelineDescription {
        struct DescriptorSet {
            enum class Type {
                Autofill,
                Empty,
                Camera,
                Materials,
                Lights,
                Debug,
                Viewport,
                PerDraw,

                CountOf
            };

            // --
            Type type = Type::Autofill;
            std::uint32_t setID = -1;
        };

        Render::ShaderSource vertexShader;
        Render::ShaderSource fragmentShader;
        PipelineType type = PipelineType::Unknown;
        Carrot::VertexFormat vertexFormat = Carrot::VertexFormat::Invalid;
        std::map<std::string, std::uint32_t> constants;
        std::unordered_map<std::uint32_t, DescriptorSet> descriptorSets;
        std::uint32_t setCount = 0; // how many sets this pipeline uses. May be higher than descriptorSets.size() if IDs are skipped (eg sets 0, 2, and 3 are used, but not 1)

        std::uint64_t subpassIndex = -1;

        bool depthTest = true;
        bool depthWrite = true;

        bool cull = true;
        bool alphaBlending = false;

        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;

        vk::PolygonMode polygonMode = vk::PolygonMode::eFill;


        Render::ShaderSource computeShader;
        Render::ShaderSource taskShader;
        Render::ShaderSource meshShader;


        Carrot::IO::Resource originatingResource;

        explicit PipelineDescription() {};
        explicit PipelineDescription(const Carrot::IO::Resource jsonFile);
    };

    class Pipeline: public SwapchainAware, public DebugNameable {
    public:
        explicit Pipeline(Carrot::VulkanDriver& driver, const Carrot::IO::Resource pipelineDescription);
        explicit Pipeline(Carrot::VulkanDriver& driver, const PipelineDescription& description);

        /**
         * Binds pipeline + descriptor sets
         */
        void bind(vk::RenderPass pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics, std::vector<std::uint32_t> dynamicOffsets = {}) const;

        /**
         * Binds only descriptor sets
         */
        void bindOnlyDescriptorSets(const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics, std::vector<std::uint32_t> dynamicOffsets = {}) const;

        void recreateDescriptorPool(std::uint32_t imageCount);

        const vk::PushConstantRange& getPushConstant(std::string_view name) const;

    public:
        [[nodiscard]] const vk::PipelineLayout& getPipelineLayout() const;
        [[nodiscard]] const vk::DescriptorSetLayout& getDescriptorSetLayout(std::uint32_t setID) const;
        std::vector<vk::DescriptorSet> getDescriptorSets(const Render::Context& renderContext, std::uint32_t setID) const;

        VertexFormat getVertexFormat() const;

    public:
        bool checkForReloadableShaders();

    public:
        PipelineDescription& getDescription() {
            return description;
        }

    public:
        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(Window& window, int newWidth, int newHeight) override;

    protected:
        void setDebugNames(const std::string& name) override;

    public:
        static PipelineType getPipelineType(const std::string& name);

    private:
        std::vector<vk::DescriptorSet> allocateAutofillDescriptorSets(std::uint32_t setID);

        void allocateDescriptorSets();

        vk::Pipeline& getOrCreatePipelineForRenderPass(vk::RenderPass pass) const;

        void reloadShaders();

        void createGraphicsTemplate();
        void createComputeTemplate();

    private:
        struct {
            std::vector<uint32_t> specializationData{}; // TODO: support something else than uint32
            std::vector<vk::SpecializationMapEntry> specializationEntries;
            vk::SpecializationInfo specialization;
        } commonPipelineTemplate;

        struct {
            std::vector<vk::VertexInputBindingDescription> vertexBindingDescriptions;
            std::vector<vk::VertexInputAttributeDescription> vertexAttributes;

            vk::PipelineVertexInputStateCreateInfo vertexInput;
            vk::PipelineInputAssemblyStateCreateInfo inputAssembly;

            vk::PipelineRasterizationStateCreateInfo rasterizer;
            vk::PipelineMultisampleStateCreateInfo multisampling;

            vk::PipelineDepthStencilStateCreateInfo depthStencil;
            std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments;

            vk::PipelineColorBlendStateCreateInfo colorBlending;
            std::vector<vk::PipelineShaderStageCreateInfo> shaderStageCreation;

            vk::DynamicState dynamicStates[2] = {
                    vk::DynamicState::eScissor,
                    vk::DynamicState::eViewport,
            };
            vk::PipelineDynamicStateCreateInfo dynamicStateInfo;

            vk::Viewport viewport;
            vk::Rect2D scissor;
            vk::PipelineViewportStateCreateInfo viewportState;


            vk::GraphicsPipelineCreateInfo pipelineInfo;
        } graphicsPipelineTemplate;

        struct {
            vk::PipelineShaderStageCreateInfo shaderStageCreation;
            vk::ComputePipelineCreateInfo pipelineInfo;
        } computePipelineTemplate;

        std::string debugName;
        Carrot::VulkanDriver& driver;
        vk::UniquePipelineLayout layout{};
        vk::UniqueDescriptorPool descriptorPool{};
        std::unique_ptr<ShaderStages> stages = nullptr;
        std::vector<std::vector<vk::DescriptorSet>> descriptorSets{};
        std::vector<vk::UniqueDescriptorSetLayout> descriptorSetLayouts{};

        PipelineDescription description;
        mutable std::unordered_map<vk::RenderPass, vk::UniquePipeline> vkPipelines{};

        mutable std::unordered_map<std::string, vk::PushConstantRange> pushConstantMap{};
    };
}
