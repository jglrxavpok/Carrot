//
// Created by jglrxavpok on 30/11/2020.
//

#include "ShaderModule.h"
#include "io/IO.h"
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_parser.hpp>
#include <iostream>

Carrot::ShaderModule::ShaderModule(Carrot::Engine& engine, const string& filename, const string& entryPoint): engine(engine), entryPoint(entryPoint) {
    auto code = IO::readFile(filename);
    auto& device = engine.getLogicalDevice();
    vkModule = device.createShaderModuleUnique({
            .codeSize = static_cast<uint32_t>(code.size()),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    }, engine.getAllocator());

    spirv_cross::Parser parser{reinterpret_cast<const uint32_t*>(code.data()), code.size()/sizeof(uint32_t)};
    parser.parse();
    parsedCode = parser.get_parsed_ir();
}

vk::PipelineShaderStageCreateInfo Carrot::ShaderModule::createPipelineShaderStage(vk::ShaderStageFlagBits stage) const {
    return {
        .stage = stage,
        .module = *vkModule,
        .pName = entryPoint.c_str(),
    };
}

void Carrot::ShaderModule::addBindings(vk::ShaderStageFlagBits stage, vector<vk::DescriptorSetLayoutBinding>& bindings) const {
    spirv_cross::Compiler compiler{parsedCode};
    const auto resources = compiler.get_shader_resources();

    // buffers
    createBindings(compiler, stage, bindings, vk::DescriptorType::eUniformBufferDynamic, resources.uniform_buffers);

    // samplers
    createBindings(compiler, stage, bindings, vk::DescriptorType::eSampler, resources.separate_samplers);

    // sampled images
    createBindings(compiler, stage, bindings, vk::DescriptorType::eSampledImage, resources.sampled_images);
    createBindings(compiler, stage, bindings, vk::DescriptorType::eSampledImage, resources.separate_images);

    // TODO: other types
}

void Carrot::ShaderModule::createBindings(spirv_cross::Compiler& compiler,
                                          vk::ShaderStageFlagBits stage,
                                          vector<vk::DescriptorSetLayoutBinding>& bindings,
                                          vk::DescriptorType type,
                                          const spirv_cross::SmallVector<spirv_cross::Resource>& resources) const {
    for(const auto& resource : resources) {
        const auto bindingID = compiler.get_decoration(resource.id, spv::DecorationBinding);
        const auto& resourceType = compiler.get_type(resource.type_id);
        uint32_t count = 1;
        if(!resourceType.array.empty()) {
            count = resourceType.array[0];
        }
        bindings.push_back(vk::DescriptorSetLayoutBinding {
            .binding = bindingID,
            .descriptorType = type,
            .descriptorCount = count,
            .stageFlags = stage,
        });
    }
}
