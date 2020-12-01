//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once
#include <string>
#include "vulkan/includes.h"
#include "Engine.h"
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_parser.hpp>

using namespace std;

namespace Carrot {
    class ShaderModule {
    private:
        Carrot::Engine& engine;
        vk::UniqueShaderModule vkModule{};
        string entryPoint = "main";
        spirv_cross::ParsedIR parsedCode{};

    public:
        explicit ShaderModule(Carrot::Engine& engine, const string& filename, const string& entryPoint = "main");

        [[nodiscard]] vk::PipelineShaderStageCreateInfo createPipelineShaderStage(vk::ShaderStageFlagBits stage) const;

        void addBindings(vk::ShaderStageFlagBits stage, vector<vk::DescriptorSetLayoutBinding>& bindings) const;

        void
        createBindings(spirv_cross::Compiler& compiler, vk::ShaderStageFlagBits stage, vector<vk::DescriptorSetLayoutBinding>& bindings,
                       vk::DescriptorType type,
                       const spirv_cross::SmallVector<spirv_cross::Resource>& resources) const;
    };
}
