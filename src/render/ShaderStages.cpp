//
// Created by jglrxavpok on 30/11/2020.
//

#include "ShaderStages.h"

Carrot::ShaderStages::ShaderStages(Carrot::Engine& engine, const vector<string>& filenames): engine(engine) {
    for(const string& filename : filenames) {
        auto stage = static_cast<vk::ShaderStageFlagBits>(0);
        if(filename.ends_with(".vertex.glsl.spv")) {
            stage = vk::ShaderStageFlagBits::eVertex;
        }
        if(filename.ends_with(".fragment.glsl.spv")) {
            stage = vk::ShaderStageFlagBits::eFragment;
        }
        stages[stage] = move(make_unique<ShaderModule>(engine, filename));
    }
}

vector<vk::PipelineShaderStageCreateInfo> Carrot::ShaderStages::createPipelineShaderStages(const vk::SpecializationInfo* specialization) const {
    vector<vk::PipelineShaderStageCreateInfo> creates{};
    for(const auto& [stage, module] : stages) {
        creates.push_back(module->createPipelineShaderStage(stage, specialization));
    }
    return creates;
}

vk::UniqueDescriptorSetLayout Carrot::ShaderStages::createDescriptorSetLayout(const map<string, uint32_t>& constants) const {
    auto& device = engine.getLogicalDevice();

    vector<vk::DescriptorSetLayoutBinding> bindings{};

    for(const auto& [stage, module] : stages) {
        module->addBindings(stage, bindings, constants);
    }

    vk::DescriptorSetLayoutCreateInfo createInfo{
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
    };

    return device.createDescriptorSetLayoutUnique(createInfo, engine.getAllocator());
}

const map<vk::ShaderStageFlagBits, unique_ptr<Carrot::ShaderModule>>& Carrot::ShaderStages::getModuleMap() const {
    return stages;
}
