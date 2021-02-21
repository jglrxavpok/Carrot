//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once
#include <vector>
#include <string>
#include "Engine.h"
#include "ShaderModule.h"
#include <map>

using namespace std;

namespace Carrot {
    class ShaderStages {
    private:
        Carrot::Engine& engine;
        vector<pair<vk::ShaderStageFlagBits, unique_ptr<ShaderModule>>> stages{};

    public:
        explicit ShaderStages(Carrot::Engine& engine, const vector<string>& filenames);

        [[nodiscard]] vector<vk::PipelineShaderStageCreateInfo> createPipelineShaderStages(const vk::SpecializationInfo* specialization = nullptr) const;

        vk::UniqueDescriptorSetLayout createDescriptorSetLayout0(const map<string, uint32_t>& constants) const;

        [[nodiscard]] const vector<pair<vk::ShaderStageFlagBits, unique_ptr<ShaderModule>>>& getModuleMap() const;
    };

}
