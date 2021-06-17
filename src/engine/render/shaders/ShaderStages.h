//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once
#include <vector>
#include <string>
#include "engine/vulkan/VulkanDriver.h"
#include "ShaderModule.h"
#include <map>

using namespace std;

namespace Carrot {
    class ShaderStages {
    private:
        Carrot::VulkanDriver& driver;
        vector<pair<vk::ShaderStageFlagBits, unique_ptr<Carrot::ShaderModule>>> stages{};

    public:
        explicit ShaderStages(Carrot::VulkanDriver& driver, const vector<string>& filenames);
        explicit ShaderStages(Carrot::VulkanDriver& driver, const vector<Carrot::IO::Resource>& filenames, const vector<vk::ShaderStageFlagBits>& stages);

        [[nodiscard]] vector<vk::PipelineShaderStageCreateInfo> createPipelineShaderStages(const vk::SpecializationInfo* specialization = nullptr) const;

        vk::UniqueDescriptorSetLayout createDescriptorSetLayout0(const map<string, uint32_t>& constants) const;

        [[nodiscard]] const vector<pair<vk::ShaderStageFlagBits, unique_ptr<Carrot::ShaderModule>>>& getModuleMap() const;
    };

}
