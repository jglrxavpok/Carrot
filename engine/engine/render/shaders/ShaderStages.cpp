//
// Created by jglrxavpok on 30/11/2020.
//

#include "ShaderStages.h"

#include <core/io/Logging.hpp>
#include <engine/vulkan/VulkanDriver.h>

Carrot::ShaderStages::ShaderStages(
    const Carrot::Vector<Render::ShaderSource>& resources,
    const Carrot::Vector<vk::ShaderStageFlagBits>& stages,
    const Carrot::Vector<std::string>& entryPoints
    ) {
    verify(resources.size() == stages.size(), "mismatched sizes");
    verify(resources.size() == entryPoints.size(), "mismatched sizes");
    for(size_t i = 0; i < resources.size(); i++) {
        this->stages.emplace_back(std::make_pair(stages[i], std::move(std::make_unique<ShaderModule>(resources[i], entryPoints[i]))));
    }
}

void Carrot::ShaderStages::reload() {
    for(const auto& [stage, module] : stages) {
        module->reload();
    }
}

std::vector<vk::PipelineShaderStageCreateInfo> Carrot::ShaderStages::createPipelineShaderStages(const vk::SpecializationInfo* specialization) const {
    std::vector<vk::PipelineShaderStageCreateInfo> creates{};
    for(const auto& [stage, module] : stages) {
        creates.push_back(module->createPipelineShaderStage(stage, specialization));
    }
    return creates;
}

vk::UniqueDescriptorSetLayout Carrot::ShaderStages::createDescriptorSetLayout(std::uint32_t setID, const std::map<std::string, std::uint32_t>& constants) const {
    auto& device = GetVulkanDriver().getLogicalDevice();

    std::vector<NamedBinding> bindings{};

    for(const auto& [stage, module] : stages) {
        module->addBindingsSet(stage, setID, bindings, constants);
    }

    std::vector<vk::DescriptorSetLayoutBinding> vkBindings{bindings.size()};
    for (int i = 0; i < vkBindings.size(); ++i) {
        if (bindings[i].vkBinding.descriptorCount == 0) {
            bindings[i].vkBinding.descriptorCount = 255;
            Carrot::Log::warn("Setting descriptor count of set %u, binding %d to 255 instead of 0", setID, i);
        }
        vkBindings[i] = bindings[i].vkBinding;
    }
    vk::DescriptorSetLayoutCreateInfo createInfo{
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = vkBindings.data(),
    };

    return device.createDescriptorSetLayoutUnique(createInfo, GetVulkanDriver().getAllocationCallbacks());
}

const std::vector<std::pair<vk::ShaderStageFlagBits, std::unique_ptr<Carrot::ShaderModule>>>& Carrot::ShaderStages::getModuleMap() const {
    return stages;
}

bool Carrot::ShaderStages::shouldReload() const {
    for(const auto& [stage, module] : stages) {
        if (module->canBeHotReloaded()) {
            return true;
        }
    }
    return false;
}
