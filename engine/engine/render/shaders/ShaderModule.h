//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once

#include <spirv_cross.hpp>
#include <spirv_parser.hpp>
#include <string>
#include <map>
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

        void addPushConstants(vk::ShaderStageFlagBits stage, std::unordered_map<std::string, vk::PushConstantRange>& pushConstants) const;

        bool canBeHotReloaded() const;

    private:
        vk::UniqueShaderModule vkModule{};
        std::string entryPoint = "main";
        spirv_cross::ParsedIR parsedCode{};
        std::unique_ptr<spirv_cross::Compiler> compiler = nullptr;
        std::map<std::uint32_t, Binding> bindingMap{};
        Render::ShaderSource source;

        void
        createBindingsSet(vk::ShaderStageFlagBits stage,
                          std::uint32_t setID,
                          std::vector<NamedBinding>& bindings,
                          vk::DescriptorType type,
                          const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                          const std::map<std::string, uint32_t>& constants);

        void reload();

        friend class ShaderStages;

    };
}
