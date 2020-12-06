//
// Created by jglrxavpok on 01/12/2020.
//

#pragma once
#include "Engine.h"
#include "vulkan/includes.h"
#include "render/shaders/ShaderStages.h"

namespace Carrot {
    class Pipeline {
    private:
        Carrot::Engine& engine;
        vk::UniqueRenderPass& renderPass;
        vk::UniqueDescriptorSetLayout descriptorSetLayout{};
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

        vector<vk::DescriptorSet> allocateDescriptorSets();

    public:
        explicit Pipeline(Carrot::Engine& engine, vk::UniqueRenderPass& renderPass, const string& pipelineName);

        void bind(uint32_t imageIndex, vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics) const;

        [[nodiscard]] const vk::PipelineLayout& getPipelineLayout() const;
        [[nodiscard]] const vk::DescriptorSetLayout& getDescriptorSetLayout() const;

        void bindDescriptorSets(vk::CommandBuffer& commands, const vector<vk::DescriptorSet>& descriptors, const vector<uint32_t>& dynamicOffsets) const;

        void recreateDescriptorPool(uint32_t imageCount);

        const vector<vk::DescriptorSet>& getDescriptorSets() const;

        MaterialID reserveMaterialSlot(const Material& material);

        TextureID reserveTextureSlot(const vk::UniqueImageView& textureView);

        void updateMaterial(const Material& material);
        void updateMaterial(const Material& material, MaterialID materialID);
    };
}
