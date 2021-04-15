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

    class Pipeline: public SwapchainAware {
        enum class Type {
            GBuffer,
            GResolve,
            Skybox,
            Blit,
            Unknown
        };

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
        vector<vk::DescriptorSet> descriptorSets{};
        MaterialID materialID = 0;
        MaterialID maxMaterialID = 16;

        TextureID textureID = 0;
        TextureID maxTextureID = 16;

        uint32_t texturesBindingIndex = 0;
        uint32_t materialsBindingIndex = 0;
        unique_ptr<Carrot::Buffer> materialStorageBuffer = nullptr;
        map<string, uint32_t> constants{};
        unordered_map<TextureID, vk::ImageView> reservedTextures{};
        uint64_t subpassIndex = 0;
        Type type = Type::Unknown;

        VertexFormat vertexFormat = VertexFormat::Invalid;

        vector<vk::DescriptorSet> allocateDescriptorSets0();

        void allocateDescriptorSets();

        void updateTextureReservation(const vk::ImageView& textureView, TextureID id, size_t imageIndex);

        static Type getPipelineType(const string& name);

    public:
        explicit Pipeline(Carrot::VulkanDriver& driver, vk::RenderPass& renderPass, const string& pipelineName);

        void bind(uint32_t imageIndex, vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics) const;

        [[nodiscard]] const vk::PipelineLayout& getPipelineLayout() const;
        [[nodiscard]] const vk::DescriptorSetLayout& getDescriptorSetLayout() const;

        void bindDescriptorSets(vk::CommandBuffer& commands, const vector<vk::DescriptorSet>& descriptors, const vector<uint32_t>& dynamicOffsets, uint32_t firstSet = 0) const;

        void recreateDescriptorPool(uint32_t imageCount);

        const vector<vk::DescriptorSet>& getDescriptorSets() const;

        VertexFormat getVertexFormat() const;

        MaterialID reserveMaterialSlot(const Material& material);

        TextureID reserveTextureSlot(const vk::UniqueImageView& textureView);

        void updateMaterial(const Material& material);
        void updateMaterial(const Material& material, MaterialID materialID);

        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;
    };
}
