//
// Created by jglrxavpok on 30/11/2020.
//

#include "ShaderStages.h"

Carrot::ShaderStages::ShaderStages(Carrot::VulkanDriver& driver, const vector<string>& filenames): driver(driver) {
    for(const string& filename : filenames) {
        auto stage = static_cast<vk::ShaderStageFlagBits>(0);
        if(filename.ends_with(".vertex.glsl.spv")) {
            stage = vk::ShaderStageFlagBits::eVertex;
        }
        if(filename.ends_with(".fragment.glsl.spv")) {
            stage = vk::ShaderStageFlagBits::eFragment;
        }
        if(filename.ends_with(".rchit.spv")) {
            stage = vk::ShaderStageFlagBits::eClosestHitKHR;
        }
        if(filename.ends_with(".rmiss.spv")) {
            stage = vk::ShaderStageFlagBits::eMissKHR;
        }
        if(filename.ends_with(".rgen.spv")) {
            stage = vk::ShaderStageFlagBits::eRaygenKHR;
        }
        stages.emplace_back(make_pair(stage, move(make_unique<ShaderModule>(driver, filename))));
    }
}

vector<vk::PipelineShaderStageCreateInfo> Carrot::ShaderStages::createPipelineShaderStages(const vk::SpecializationInfo* specialization) const {
    vector<vk::PipelineShaderStageCreateInfo> creates{};
    for(const auto& [stage, module] : stages) {
        creates.push_back(module->createPipelineShaderStage(stage, specialization));
    }
    return creates;
}

vk::UniqueDescriptorSetLayout Carrot::ShaderStages::createDescriptorSetLayout0(const map<string, uint32_t>& constants) const {
    auto& device = driver.getLogicalDevice();

    vector<NamedBinding> bindings{};

    for(const auto& [stage, module] : stages) {
        module->addBindingsSet0(stage, bindings, constants);
    }

    vector<vk::DescriptorSetLayoutBinding> vkBindings{bindings.size()};
    for (int i = 0; i < vkBindings.size(); ++i) {
        vkBindings[i] = bindings[i].vkBinding;
    }
    vk::DescriptorSetLayoutCreateInfo createInfo{
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = vkBindings.data(),
    };

    return device.createDescriptorSetLayoutUnique(createInfo, driver.getAllocationCallbacks());
}

const vector<pair<vk::ShaderStageFlagBits, unique_ptr<Carrot::ShaderModule>>>& Carrot::ShaderStages::getModuleMap() const {
    return stages;
}
