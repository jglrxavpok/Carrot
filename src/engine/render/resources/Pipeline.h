//
// Created by jglrxavpok on 01/12/2020.
//

#pragma once
#include "engine/vulkan/includes.h"
#include "engine/vulkan/VulkanDriver.h"
#include "engine/vulkan/SwapchainAware.h"
#include "engine/render/IDTypes.h"
#include "engine/render/shaders/ShaderStages.h"
#include "VertexFormat.h"

namespace Carrot {
    class Material;

    class Buffer;

    enum class PipelineType {
        GBuffer,
        GResolve,
        Skybox,
        Blit,
        Particles,
        Unknown
    };

    struct PipelineDescription {
        Carrot::IO::Resource vertexShader;
        Carrot::IO::Resource fragmentShader;
        PipelineType type = PipelineType::Unknown;
        Carrot::VertexFormat vertexFormat = Carrot::VertexFormat::Invalid;
        std::map<std::string, std::uint32_t> constants;

        std::uint64_t texturesBindingIndex = -1;
        std::uint64_t materialStorageBufferBindingIndex = -1;
        std::uint64_t subpassIndex = -1;

        explicit PipelineDescription() {};
        explicit PipelineDescription(const Carrot::IO::Resource jsonFile);
    };

    class Pipeline: public SwapchainAware {

    private:
        Carrot::VulkanDriver& driver;
        vk::RenderPass& renderPass;
        vk::UniqueDescriptorSetLayout descriptorSetLayout0{};
        /// can be nullptr, used for animations
        vk::UniqueDescriptorSetLayout descriptorSetLayout1{};
        vk::UniquePipelineLayout layout{};
        vk::UniquePipeline vkPipeline{};
        vk::UniqueDescriptorPool descriptorPool{};
        unique_ptr<ShaderStages> stages = nullptr;
        vector<vk::DescriptorSet> descriptorSets0{};
        MaterialID materialID = 0;
        MaterialID maxMaterialID = 16;

        TextureID textureID = 0;
        TextureID maxTextureID = 16;

        PipelineDescription description;
        unique_ptr<Carrot::Buffer> materialStorageBuffer = nullptr;
        unordered_map<TextureID, vk::ImageView> reservedTextures{};

        vector<vk::DescriptorSet> allocateDescriptorSets0();

        void allocateDescriptorSets();

        void updateTextureReservation(const vk::ImageView& textureView, TextureID id, size_t imageIndex);

    public:
        explicit Pipeline(Carrot::VulkanDriver& driver, vk::RenderPass& renderPass, const Carrot::IO::Resource pipelineDescription);
        explicit Pipeline(Carrot::VulkanDriver& driver, vk::RenderPass& renderPass, const PipelineDescription description);

        void bind(uint32_t imageIndex, vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics) const;

        void bindDescriptorSets(vk::CommandBuffer& commands, const vector<vk::DescriptorSet>& descriptors, const vector<uint32_t>& dynamicOffsets, uint32_t firstSet = 0) const;

        void recreateDescriptorPool(uint32_t imageCount);

    public:
        [[nodiscard]] const vk::PipelineLayout& getPipelineLayout() const;
        [[nodiscard]] const vk::DescriptorSetLayout& getDescriptorSetLayout() const;
        const vector<vk::DescriptorSet>& getDescriptorSets0() const;

        VertexFormat getVertexFormat() const;

    public:
        MaterialID reserveMaterialSlot(const Material& material);

        TextureID reserveTextureSlot(const vk::ImageView& textureView);

        void updateMaterial(const Material& material);
        void updateMaterial(const Material& material, MaterialID materialID);

    public:
        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    public:
        static PipelineType getPipelineType(const string& name);
    };
}
