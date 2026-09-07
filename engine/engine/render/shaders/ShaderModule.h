//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once

#include <string>
#include <map>
#include <core/containers/Vector.hpp>
#include <engine/render/NamedBinding.h>
#include <core/io/Resource.h>
#include "engine/render/shaders/ShaderSource.h"

namespace Carrot {
    class ShaderModule {
    public:
        struct Binding {
            std::uint32_t index;
            vk::DescriptorType type;
            std::uint32_t count;

            auto operator<=>(const Binding& rhs) const = default;
        };

    public:
        explicit ShaderModule(const Render::ShaderSource& source, const std::string& entryPoint = "main");

        [[nodiscard]] vk::PipelineShaderStageCreateInfo createPipelineShaderStage(vk::ShaderStageFlagBits stage, const vk::SpecializationInfo* specialization) const;

        void addBindingsSet(vk::ShaderStageFlagBits stage, std::uint32_t setID, std::vector<NamedBinding>& bindings, const std::map<std::string, uint32_t>& constants);

        void addPushConstantInfo(vk::ShaderStageFlagBits stage, vk::PushConstantRange& pushConstant) const;

        bool canBeHotReloaded() const;

    private:
        vk::UniqueShaderModule vkModule{};
        std::string entryPoint = "main";
        std::unordered_map<u32/*set ID*/, Carrot::Vector<NamedBinding>> usedBindings;
        Render::ShaderSource source;
        u32 pushConstantRangeStart = 0;
        u32 pushConstantRangeSize = 0;

        void reload();

        friend class ShaderStages;

    };
}
