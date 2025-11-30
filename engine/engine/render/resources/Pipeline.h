//
// Created by jglrxavpok on 01/12/2020.
//

#pragma once
#include <core/data/ShaderMetadata.h>
#include <core/render/ImageFormats.h>

#include "engine/vulkan/SwapchainAware.h"
#include "engine/render/IDTypes.h"
#include "engine/render/shaders/ShaderStages.h"
#include "VertexFormat.h"
#include "engine/render/shaders/ShaderSource.h"
#include <core/utils/Lookup.hpp>

#include "engine/vulkan/DebugNameable.h"

namespace Carrot {
    class BufferView;
}

namespace Carrot::Render {
    class CompiledPass;
}

namespace Carrot {
    struct RenderingPipelineCreateInfo;
}

template<>
struct std::hash<Carrot::RenderingPipelineCreateInfo> {
    std::size_t operator()(const Carrot::RenderingPipelineCreateInfo& info) const noexcept;
};

namespace Carrot {
    namespace Render {
        struct Context;
    }

    class Material;

    class Buffer;
    class VulkanDriver;

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
                Manual,
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
            Type type = Type::Manual;
            std::uint32_t setID = -1;
        };

        Render::ShaderSource vertexShader;
        Render::ShaderSource fragmentShader;
        PipelineType type = PipelineType::Unknown;
        Carrot::VertexFormat vertexFormat = Carrot::VertexFormat::Invalid;
        std::map<std::string, std::uint32_t> constants;
        std::unordered_map<std::uint32_t, DescriptorSet> descriptorSets;
        std::unordered_map<std::string /*maybe change to precomputed hash if needed in future*/, ShaderCompiler::BindingSlot> descriptorReflection;
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

    // Intended as a wrapper / renderer-agnostic version of VkPipelineRenderingCreateInfo
    struct RenderingPipelineCreateInfo {
        Carrot::Vector<vk::Format> colorAttachments;
        vk::Format depthFormat = vk::Format::eUndefined;
        vk::Format stencilFormat = vk::Format::eUndefined;

        bool operator==(const RenderingPipelineCreateInfo&) const = default;
    };

    class Pipeline: public SwapchainAware, public DebugNameable {
    public:
        explicit Pipeline(Carrot::VulkanDriver& driver, const Carrot::IO::Resource pipelineDescription);
        explicit Pipeline(Carrot::VulkanDriver& driver, const PipelineDescription& description);

        /**
         * Binds pipeline + descriptor sets
         */
        void bind(const RenderingPipelineCreateInfo& createInfo, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics, const std::vector<std::uint32_t>& dynamicOffsets = {}) const;
        void bind(const Render::CompiledPass& pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics, const std::vector<std::uint32_t>& dynamicOffsets = {}) const;

        /**
         * Binds only descriptor sets
         */
        void bindOnlyDescriptorSets(const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics, const std::vector<std::uint32_t>& dynamicOffsets = {}) const;

        void recreateDescriptorPool(std::uint32_t imageCount);

        const vk::PushConstantRange& getPushConstant(std::string_view name) const;

        /**
         * Does this pipeline have the given binding slot?
         */
        bool hasBinding(u32 setID, u32 bindingID) const;

    public: // pass data to pipeline
        void setStorageBuffer(const Carrot::Render::Context& renderContext, const std::string& slotName, const Carrot::BufferView& buffer);

    public:
        [[nodiscard]] const vk::PipelineLayout& getPipelineLayout() const;
        [[nodiscard]] const vk::DescriptorSetLayout& getDescriptorSetLayout(std::uint32_t setID) const;
        std::vector<vk::DescriptorSet> getDescriptorSets(const Render::Context& renderContext, std::uint32_t setID) const;

        VertexFormat getVertexFormat() const;

    public:
        bool checkForReloadableShaders();

        /**
         * Gets the 'generation number' of the pipeline, which means how many this pipeline was regenerated
         */
        u32 getGenerationNumber() const;

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
        std::vector<vk::DescriptorSet> allocateManualDescriptorSets(std::uint32_t setID);

        void allocateDescriptorSets();

        vk::Pipeline& getOrCreatePipelineForRendering(const RenderingPipelineCreateInfo& createInfo) const;

        void reloadShaders(bool needDeviceWait);

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
        std::unordered_map<u32, std::unordered_set<u32>> presentBindings;

        u32 generationNumber = 0;

        PipelineDescription description;
        mutable std::unordered_map<RenderingPipelineCreateInfo, vk::UniquePipeline> vkPipelines{}; // for dynamic rendering

        mutable std::unordered_map<std::string, vk::PushConstantRange> pushConstantMap{};
    };
}
