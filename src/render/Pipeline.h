//
// Created by jglrxavpok on 01/12/2020.
//

#pragma once
#include "Engine.h"
#include "vulkan/includes.h"

namespace Carrot {
    class Pipeline {
    private:
        Carrot::Engine& engine;
        vk::UniqueRenderPass& renderPass;
        vk::UniqueDescriptorSetLayout descriptorSetLayout{};
        vk::UniquePipelineLayout layout{};
        vk::UniquePipeline vkPipeline{};


    public:
        explicit Pipeline(Carrot::Engine& engine, vk::UniqueRenderPass& renderPass, const string& shaderName);

        void bind(vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics) const;

        [[nodiscard]] const vk::PipelineLayout& getPipelineLayout() const;
        [[nodiscard]] const vk::DescriptorSetLayout& getDescriptorSetLayout() const;

        void bindDescriptorSets(vk::CommandBuffer& commands, const vector<vk::DescriptorSet>& descriptors, const vector<uint32_t>& dynamicOffsets) const;
    };
}
