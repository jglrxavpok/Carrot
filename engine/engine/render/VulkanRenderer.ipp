//
// Created by jglrxavpok on 09/07/2021.
//

#pragma once
#include "VulkanRenderer.h"

namespace Carrot {
    template<typename ConstantBlock>
    void VulkanRenderer::pushConstantBlock(std::string_view pushName, const Carrot::Pipeline& pipeline, const Carrot::Render::Context& context, vk::ShaderStageFlags stageFlags, vk::CommandBuffer& cmds, const ConstantBlock& block) {
        const auto& layout = pipeline.getPipelineLayout();
        auto copy = std::make_unique<std::uint8_t[]>(sizeof(ConstantBlock));
        std::memcpy(copy.get(), &block, sizeof(ConstantBlock));
        pushConstantList.emplace_back(std::move(copy));
        const auto& range = pipeline.getPushConstant(pushName);
        cmds.pushConstants(layout, stageFlags, range.offset, sizeof(ConstantBlock), pushConstantList.back().get());
    }

    template<typename... ConstantTypes>
    void VulkanRenderer::pushConstants(std::string_view pushName, const Carrot::Pipeline& pipeline, const Carrot::Render::Context& context, vk::ShaderStageFlags stageFlags, vk::CommandBuffer& cmds, ConstantTypes&&... args) {
        const auto& layout = pipeline.getPipelineLayout();
        const std::size_t totalSize = []() {
            std::size_t s = 0;
            ((s += sizeof(ConstantTypes)), ...);
            return s;
        }();
        auto copy = std::make_unique<std::uint8_t[]>(totalSize);

        std::uint8_t* pDest = copy.get();
        ((
            std::memcpy(pDest, &args, sizeof(ConstantTypes)),
            pDest += sizeof(ConstantTypes)
        ), ...);
        pushConstantList.emplace_back(std::move(copy));
        const auto& range = pipeline.getPushConstant(pushName);
        cmds.pushConstants(layout, stageFlags, range.offset, totalSize, pushConstantList.back().get());
    }

}