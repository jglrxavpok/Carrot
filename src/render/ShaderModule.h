//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once
#include <string>
#include "vulkan/includes.h"
#include "Engine.h"
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_parser.hpp>
#include <map>

using namespace std;

namespace Carrot {
    class ShaderModule {
    public:
        struct Binding {
            uint32_t index;
            vk::DescriptorType type;
            uint32_t count;
        };

    private:
        Carrot::Engine& engine;
        vk::UniqueShaderModule vkModule{};
        string entryPoint = "main";
        spirv_cross::ParsedIR parsedCode{};
        unique_ptr<spirv_cross::Compiler> compiler = nullptr;
        map<uint32_t, Binding> bindingMap{};

        void
        createBindings(vk::ShaderStageFlagBits stage, vector<vk::DescriptorSetLayoutBinding>& bindings,
                       vk::DescriptorType type,
                       const spirv_cross::SmallVector<spirv_cross::Resource>& resources);

    public:
        explicit ShaderModule(Carrot::Engine& engine, const string& filename, const string& entryPoint = "main");

        [[nodiscard]] vk::PipelineShaderStageCreateInfo createPipelineShaderStage(vk::ShaderStageFlagBits stage) const;

        void addBindings(vk::ShaderStageFlagBits stage, vector<vk::DescriptorSetLayoutBinding>& bindings);
    };
}
