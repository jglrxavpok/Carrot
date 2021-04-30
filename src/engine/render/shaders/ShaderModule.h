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

using namespace std;

namespace Carrot {
    class ShaderModule {
    public:
        struct Binding {
            uint32_t index;
            vk::DescriptorType type;
            uint32_t count;

            auto operator<=>(const Binding& rhs) const = default;
        };

    private:
        Carrot::VulkanDriver& driver;
        vk::UniqueShaderModule vkModule{};
        string entryPoint = "main";
        spirv_cross::ParsedIR parsedCode{};
        unique_ptr<spirv_cross::Compiler> compiler = nullptr;
        map<uint32_t, Binding> bindingMap{};

        void
        createBindingsSet0(vk::ShaderStageFlagBits stage, vector<NamedBinding>& bindings,
                           vk::DescriptorType type,
                           const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                           const map<string, uint32_t>& constants);

    public:
        explicit ShaderModule(Carrot::VulkanDriver& driver, const string& filename, const string& entryPoint = "main");

        [[nodiscard]] vk::PipelineShaderStageCreateInfo createPipelineShaderStage(vk::ShaderStageFlagBits stage, const vk::SpecializationInfo* specialization) const;

        void addBindingsSet0(vk::ShaderStageFlagBits stage, vector<NamedBinding>& bindings, const map<string, uint32_t>& constants);

        void addPushConstants(vk::ShaderStageFlagBits stage, vector<vk::PushConstantRange>& pushConstants) const;
    };
}
