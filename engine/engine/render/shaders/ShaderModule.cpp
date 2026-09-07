//
// Created by jglrxavpok on 30/11/2020.
//

#include <spirv_reflect.h>
#include "ShaderModule.h"
#include "core/io/IO.h"
#include <spirv_reflect.h>
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

    vkModule = device.createShaderModuleUnique(vk::ShaderModuleCreateInfo{
            .codeSize = static_cast<uint32_t>(code.size()),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    }, GetVulkanDriver().getAllocationCallbacks());

    source.clearModifyFlag();

    spv_reflect::ShaderModule mod{code};
    SpvReflectResult result{};
    const SpvReflectBlockVariable* pBlock = mod.GetEntryPointPushConstantBlock(entryPoint.c_str(), &result);
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        pushConstantRangeStart = pBlock->absolute_offset;
        pushConstantRangeSize = pBlock->padded_size;
    } else if (result == SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND) {
        // no push constant, and that's ok
    } else {
        verify(false, "Failed to get push constant block");
    }

    u32 setCount;
    if (result = mod.EnumerateDescriptorSets(&setCount, nullptr); result != SPV_REFLECT_RESULT_SUCCESS) {
        verify(false, "Failed to count descriptor sets");
    }

    Carrot::Vector<SpvReflectDescriptorSet*> sets;
    sets.resize(setCount);
    if (result = mod.EnumerateDescriptorSets(&setCount, sets.data()); result != SPV_REFLECT_RESULT_SUCCESS) {
        verify(false, "Failed to enumerate descriptor sets");
    }

    usedBindings.clear();
    for (const SpvReflectDescriptorSet* pSet : sets) {
        Carrot::Vector<NamedBinding>& bindings = usedBindings[pSet->set];

        const u32 bindingCount = pSet->binding_count;
        bindings.resize(bindingCount);
        for (u32 bindingIndex = 0; bindingIndex < bindingCount; bindingIndex++) {
            const SpvReflectDescriptorBinding* pBinding = pSet->bindings[bindingIndex];
            bindings[bindingIndex].name = pBinding->name;
            bindings[bindingIndex].setID = pBinding->set;
            bindings[bindingIndex].vkBinding = vk::DescriptorSetLayoutBinding{
                .binding = pBinding->binding,
                .descriptorType = static_cast<vk::DescriptorType>(pBinding->descriptor_type),
                .descriptorCount = pBinding->count, // TODO: spec constant for sizes
            };
        }
    }
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
    const auto& shaderBindings = usedBindings[setID];
    if (shaderBindings.empty()) {
        return;
    }

    for (const auto& binding : shaderBindings) {
        auto existing = std::ranges::find_if(bindings, [&](const auto& b) { return b.setID == binding.setID && b.vkBinding.binding == binding.vkBinding.binding; });
        if(existing != bindings.end()) {
            if(binding.areSame(*existing)) {
                existing->vkBinding.stageFlags |= stage;
            } else {
                throw std::runtime_error("Mismatched of binding " + std::to_string(binding.vkBinding.binding) + ", set " + std::to_string(binding.setID) +" over different stages.");
            }
        } else {
            NamedBinding bindingWithFlags = binding;
            bindingWithFlags.vkBinding.stageFlags = stage;
            bindings.push_back(bindingWithFlags);
        }
    }
}

void Carrot::ShaderModule::addPushConstantInfo(vk::ShaderStageFlagBits stage, vk::PushConstantRange& pipelinePushConstant) const {
    if (pushConstantRangeSize == 0)
        return;
    pipelinePushConstant.stageFlags |= stage;
    pipelinePushConstant.offset = 0;
    pipelinePushConstant.size = pushConstantRangeStart + pushConstantRangeSize;
}

bool Carrot::ShaderModule::canBeHotReloaded() const {
    return source.hasSourceBeenModified();
}