//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once
#include <string>
#include "engine/vulkan/includes.h"
#include "engine/vulkan/VulkanDriver.h"
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_parser.hpp>
#include <map>
#include <engine/render/shaders/Specialization.h>
#include <engine/render/NamedBinding.h>
#include <core/io/Resource.h>
#include <span>

namespace Carrot {
    class ShaderModule {
    public:
        struct Binding {
            std::uint32_t index;
            vk::DescriptorType type;
            std::uint32_t count;

            auto operator<=>(const Binding& rhs) const = default;
        };

    private:
        Carrot::VulkanDriver& driver;
        vk::UniqueShaderModule vkModule{};
        std::string entryPoint = "main";
        spirv_cross::ParsedIR parsedCode{};
        std::unique_ptr<spirv_cross::Compiler> compiler = nullptr;
        std::map<std::uint32_t, Binding> bindingMap{};

        void
        createBindingsSet0(vk::ShaderStageFlagBits stage, std::vector<NamedBinding>& bindings,
                           vk::DescriptorType type,
                           const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                           const std::map<std::string, uint32_t>& constants);

    public:
        explicit ShaderModule(Carrot::VulkanDriver& driver, const IO::Resource resource, const std::string& entryPoint = "main");
        explicit ShaderModule(Carrot::VulkanDriver& driver, const std::vector<uint8_t>& code, const std::string& entryPoint = "main");

        [[nodiscard]] vk::PipelineShaderStageCreateInfo createPipelineShaderStage(vk::ShaderStageFlagBits stage, const vk::SpecializationInfo* specialization) const;

        void addBindingsSet0(vk::ShaderStageFlagBits stage, std::vector<NamedBinding>& bindings, const std::map<std::string, uint32_t>& constants);

        void addPushConstants(vk::ShaderStageFlagBits stage, std::unordered_map<std::string, vk::PushConstantRange>& pushConstants) const;
    };
}
