//
// Created by jglrxavpok on 30/11/2020.
//

#include "ShaderModule.h"
#include "engine/io/IO.h"
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_parser.hpp>
#include <iostream>
#include "engine/render/NamedBinding.h"

Carrot::ShaderModule::ShaderModule(Carrot::VulkanDriver& driver, const string& filename, const string& entryPoint): Carrot::ShaderModule(driver, IO::readFile(filename), entryPoint) {}

Carrot::ShaderModule::ShaderModule(Carrot::VulkanDriver& driver, const vector<uint8_t>& code, const string& entryPoint): driver(driver), entryPoint(entryPoint) {
    auto& device = driver.getLogicalDevice();
    vkModule = device.createShaderModuleUnique(vk::ShaderModuleCreateInfo{
            .codeSize = static_cast<uint32_t>(code.size()),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    }, driver.getAllocationCallbacks());

    spirv_cross::Parser parser{reinterpret_cast<const uint32_t*>(code.data()), code.size()/sizeof(uint32_t)};
    parser.parse();
    parsedCode = parser.get_parsed_ir();

    compiler = make_unique<spirv_cross::Compiler>(parsedCode);
}

vk::PipelineShaderStageCreateInfo Carrot::ShaderModule::createPipelineShaderStage(vk::ShaderStageFlagBits stage, const vk::SpecializationInfo* specialization) const {
    return {
        .stage = stage,
        .module = *vkModule,
        .pName = entryPoint.c_str(),
        .pSpecializationInfo = specialization,
    };
}

void Carrot::ShaderModule::addBindingsSet0(vk::ShaderStageFlagBits stage, vector<NamedBinding>& bindings, const map<string, uint32_t>& constants) {
    const auto resources = compiler->get_shader_resources();

    // buffers
    createBindingsSet0(stage, bindings, vk::DescriptorType::eUniformBufferDynamic, resources.uniform_buffers, constants);

    // samplers
    createBindingsSet0(stage, bindings, vk::DescriptorType::eSampler, resources.separate_samplers, constants);

    // sampled images
    createBindingsSet0(stage, bindings, vk::DescriptorType::eCombinedImageSampler, resources.sampled_images, constants);
    createBindingsSet0(stage, bindings, vk::DescriptorType::eSampledImage, resources.separate_images, constants);


    createBindingsSet0(stage, bindings, vk::DescriptorType::eInputAttachment, resources.subpass_inputs, constants);


    createBindingsSet0(stage, bindings, vk::DescriptorType::eStorageBufferDynamic, resources.storage_buffers, constants);

    // TODO: other types
}

void Carrot::ShaderModule::createBindingsSet0(vk::ShaderStageFlagBits stage,
                                              vector<NamedBinding>& bindings,
                                              vk::DescriptorType type,
                                              const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                                              const map<string, uint32_t>& constants) {
    for(const auto& resource : resources) {
        const auto bindingID = compiler->get_decoration(resource.id, spv::DecorationBinding);
        const auto& resourceType = compiler->get_type(resource.type_id);
        const auto setIndex = compiler->get_decoration(resource.id, spv::DecorationDescriptorSet);
        if(setIndex != 0) { // TODO: avoid manually referencing non-0 sets
            continue;
        }
        uint32_t count = 1;
        if(!resourceType.array.empty()) {
            bool isLiteralLength = resourceType.array_size_literal[0];
            if(isLiteralLength) {
                count = resourceType.array[0];
            } else {
                // resolve specialization constant value
                uint32_t varId = resourceType.array[0];
                const auto& constant = compiler->get_constant(varId);
                const string constantName = compiler->get_name(varId);
                auto it = constants.find(constantName);
                if(it != constants.end()) {
                    count = it->second;
                } else {
                    count = constant.scalar(); // default value
                }
            }
        }
        NamedBinding bindingToAdd = {vk::DescriptorSetLayoutBinding {
                .binding = bindingID,
                .descriptorType = type,
                .descriptorCount = count,
                .stageFlags = stage,
        }, resource.name};

        auto existing = std::find_if(bindings.begin(), bindings.end(), [&](const auto& b) { return b.name == bindingToAdd.name; });
        if(existing != bindings.end()) {
            if(bindingToAdd.areSame(*existing)) {
                existing->vkBinding.stageFlags |= stage;
            } else {
                throw std::runtime_error("Mismatched of binding " + to_string(bindingID) + " over different stages.");
            }
        } else {
            bindings.push_back(bindingToAdd);

            bindingMap[static_cast<uint32_t>(bindingID)] = {
                    bindingID,
                    type,
                    count
            };
        }
    }
}

void Carrot::ShaderModule::addPushConstants(vk::ShaderStageFlagBits stage, vector<vk::PushConstantRange>& pushConstants) const {
    const auto resources = compiler->get_shader_resources();

    // TODO: other types than uint32_t
    for(const auto& pushConstant : resources.push_constant_buffers) {
        pushConstants.emplace_back(vk::PushConstantRange {
            .stageFlags = stage,
            .offset = static_cast<uint32_t>(pushConstants.size()*sizeof(uint32_t)),
            .size = sizeof(uint32_t),
        });
    }
}
