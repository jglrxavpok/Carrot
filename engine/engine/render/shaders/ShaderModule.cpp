//
// Created by jglrxavpok on 30/11/2020.
//

#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>
#include <spirv_reflect.hpp>
#include "ShaderModule.h"
#include "core/io/IO.h"
#include <iostream>
#include <core/math/BasicFunctions.h>

#include "engine/render/NamedBinding.h"
#include "engine/utils/Macros.h"
#include "core/io/Logging.hpp"
#include <engine/vulkan/VulkanDriver.h>

static Carrot::Log::Category category { "Shader" };

Carrot::ShaderModule::ShaderModule(const Render::ShaderSource& source, const std::string& entryPoint)
        : entryPoint(entryPoint), source(source) {
    reload();
}

void Carrot::ShaderModule::reload() {
    Carrot::Log::info(category, "Loading shader %s", source.getName().c_str());
    auto code = source.getCode();
    auto& device = GetVulkanDriver().getLogicalDevice();

    std::vector<std::uint32_t> asWords;
    asWords.resize(code.size() / sizeof(std::uint32_t));
    std::memcpy(asWords.data(), code.data(), code.size());
    spirv_cross::Parser parser(std::move(asWords));
    parser.parse();

    compiler = std::make_unique<spirv_cross::CompilerReflection>(std::move(parser.get_parsed_ir()));

    vkModule = device.createShaderModuleUnique(vk::ShaderModuleCreateInfo{
            .codeSize = static_cast<uint32_t>(code.size()),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    }, GetVulkanDriver().getAllocationCallbacks());

    source.clearModifyFlag();
}

vk::PipelineShaderStageCreateInfo Carrot::ShaderModule::createPipelineShaderStage(vk::ShaderStageFlagBits stage, const vk::SpecializationInfo* specialization) const {
    return {
        .stage = stage,
        .module = *vkModule,
        .pName = entryPoint.c_str(),
        .pSpecializationInfo = specialization,
    };
}

void Carrot::ShaderModule::addBindingsSet(vk::ShaderStageFlagBits stage, std::uint32_t setID, std::vector<NamedBinding>& bindings, const std::map<std::string, std::uint32_t>& constants) {
    const auto resources = compiler->get_shader_resources();

    // buffers
    createBindingsSet(stage, setID, bindings, vk::DescriptorType::eUniformBuffer, resources.uniform_buffers, constants);

    // samplers
    createBindingsSet(stage, setID, bindings, vk::DescriptorType::eSampler, resources.separate_samplers, constants);

    // sampled images
    createBindingsSet(stage, setID, bindings, vk::DescriptorType::eCombinedImageSampler, resources.sampled_images, constants);
    createBindingsSet(stage, setID, bindings, vk::DescriptorType::eSampledImage, resources.separate_images, constants);


    createBindingsSet(stage, setID, bindings, vk::DescriptorType::eInputAttachment, resources.subpass_inputs, constants);


    createBindingsSet(stage, setID, bindings, vk::DescriptorType::eStorageBuffer, resources.storage_buffers, constants);
    createBindingsSet(stage, setID, bindings, vk::DescriptorType::eStorageImage, resources.storage_images, constants);

    createBindingsSet(stage, setID, bindings, vk::DescriptorType::eAccelerationStructureKHR, resources.acceleration_structures, constants);
    //createBindingsSet0(stage, bindings, vk::DescriptorType::eAccelerationStructureNV, resources.acceleration_structures, constants);

    // TODO: other types
}

void Carrot::ShaderModule::createBindingsSet(vk::ShaderStageFlagBits stage,
                                             std::uint32_t setID,
                                             std::vector<NamedBinding>& bindings,
                                             vk::DescriptorType type,
                                             const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                                             const std::map<std::string, std::uint32_t>& constants) {
    for(const auto& resource : resources) {
        const auto bindingID = compiler->get_decoration(resource.id, spv::DecorationBinding);
        const auto& resourceType = compiler->get_type(resource.type_id);
        const auto setIndex = compiler->get_decoration(resource.id, spv::DecorationDescriptorSet);
        if(setIndex != setID) {
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
                const std::string& constantName = compiler->get_name(varId);
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
        }, setID, resource.name};

        auto existing = std::find_if(bindings.begin(), bindings.end(), [&](const auto& b) { return b.setID == bindingToAdd.setID && b.vkBinding.binding == bindingToAdd.vkBinding.binding; });
        if(existing != bindings.end()) {
            if(bindingToAdd.areSame(*existing)) {
                existing->vkBinding.stageFlags |= stage;
            } else {
                throw std::runtime_error("Mismatched of binding " + std::to_string(bindingID) + " over different stages.");
            }
        } else {
            bindings.push_back(bindingToAdd);

            bindingMap[static_cast<std::uint32_t>(bindingID)] = {
                    bindingID,
                    type,
                    count
            };
        }
    }
}

static std::uint64_t computeTypeSize(const spirv_cross::Compiler& compiler, const spirv_cross::SPIRType& type) {
    if (type.pointer) {
        return sizeof(vk::DeviceAddress);
    }
    std::uint32_t sizeMultiplier = 1;
    sizeMultiplier *= type.vecsize;
    for (std::uint32_t dimIndex = 0; dimIndex < type.array.size(); dimIndex++) {
        verify(type.array_size_literal[dimIndex], "non-literal array sizes for push constant block members are not supported yet");
        sizeMultiplier *= type.array[dimIndex];
    }
    verify(sizeMultiplier != 0, "logic error, one value is 0");
    switch(type.basetype) {
        case spirv_cross::SPIRType::Struct: {
            std::uint32_t size = 0;
            for(const auto& t : type.member_types) {
                auto& memberType = compiler.get_type(t);
                size += computeTypeSize(compiler, memberType);
            }
            return size * sizeMultiplier;
        }

        case spirv_cross::SPIRType::Float:
        case spirv_cross::SPIRType::UInt:
        case spirv_cross::SPIRType::Int:
            return 4 * sizeMultiplier;

        case spirv_cross::SPIRType::UInt64:
        case spirv_cross::SPIRType::Int64:
        case spirv_cross::SPIRType::Double:
            return 8 * sizeMultiplier;

        case spirv_cross::SPIRType::UByte:
            return 1 * sizeMultiplier;
        case spirv_cross::SPIRType::UShort:
            return 2 * sizeMultiplier;

        default:
            TODO
    }
}

void Carrot::ShaderModule::addPushConstants(vk::ShaderStageFlagBits stage, std::unordered_map<std::string, vk::PushConstantRange>& pushConstants) const {
    const auto resources = compiler->get_shader_resources();

    if(resources.push_constant_buffers.size() == 0)
        return;
    std::uint32_t offset = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t size = 0;
    const auto& pushConstant = resources.push_constant_buffers[0];
    const auto& ranges = compiler->get_active_buffer_ranges(pushConstant.id);
    std::string name = pushConstant.name;
    const auto& resourceType = compiler->get_type(pushConstant.type_id);

    std::size_t realSize = computeTypeSize(*compiler, compiler->get_type(resourceType.parent_type));

    for(const auto& r : ranges) {
        offset = Carrot::Math::alignDown(std::min(static_cast<std::uint32_t>(r.offset), offset), 4u);
        size = Carrot::Math::alignUp(std::max(static_cast<std::uint32_t>(r.offset+realSize), size), 4u);
    }

    if(pushConstants.contains(name)) {
        auto& existingRange = pushConstants.at(name);
        existingRange.stageFlags |= stage;
        existingRange.offset = Carrot::Math::alignDown(std::min(static_cast<std::uint32_t>(offset), existingRange.offset), 4u);
        existingRange.size = Carrot::Math::alignUp(std::max(static_cast<std::uint32_t>(realSize), existingRange.size), 4u);
        return;
    }

    pushConstants[name] = vk::PushConstantRange {
            .stageFlags = stage,
            .offset = offset,
            .size = size,
    };
}

bool Carrot::ShaderModule::canBeHotReloaded() const {
    return source.hasSourceBeenModified();
}