//
// Created by jglrxavpok on 09/07/2021.
//

#pragma once
#include "VulkanRenderer.h"

namespace Carrot {
    template<typename ConstantBlock>
    void VulkanRenderer::pushConstantBlock(Carrot::Pipeline& pipeline, const Carrot::Render::Context& context, vk::ShaderStageFlags stageFlags, vk::CommandBuffer& cmds, const ConstantBlock& block) {
        pushConstantBlock(pipeline.getPipelineLayout(), context, stageFlags, cmds, block);
    }

    template<typename ConstantBlock>
    void VulkanRenderer::pushConstantBlock(const vk::PipelineLayout& layout, const Carrot::Render::Context& context, vk::ShaderStageFlags stageFlags, vk::CommandBuffer& cmds, const ConstantBlock& block) {
        auto copy = std::make_unique<std::uint8_t[]>(sizeof(ConstantBlock));
        std::memcpy(copy.get(), &block, sizeof(ConstantBlock));
        pushConstants.emplace_back(std::move(copy));
        cmds.pushConstants(layout, stageFlags, 0, sizeof(ConstantBlock), pushConstants.back().get());
    }
}