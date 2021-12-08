//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once
#include <vector>
#include <string>
#include "engine/vulkan/VulkanDriver.h"
#include "ShaderModule.h"
#include <map>

namespace Carrot {
    class ShaderStages {
    private:
        Carrot::VulkanDriver& driver;
        std::vector<std::pair<vk::ShaderStageFlagBits, std::unique_ptr<Carrot::ShaderModule>>> stages{};

    public:
        explicit ShaderStages(Carrot::VulkanDriver& driver, const std::vector<std::string>& filenames);
        explicit ShaderStages(Carrot::VulkanDriver& driver, const std::vector<Render::ShaderSource>& filenames, const std::vector<vk::ShaderStageFlagBits>& stages);

    public:
        [[nodiscard]] std::vector<vk::PipelineShaderStageCreateInfo> createPipelineShaderStages(const vk::SpecializationInfo* specialization = nullptr) const;

        [[nodiscard]] vk::UniqueDescriptorSetLayout createDescriptorSetLayout0(const std::map<std::string, std::uint32_t>& constants) const;

        [[nodiscard]] const std::vector<std::pair<vk::ShaderStageFlagBits, std::unique_ptr<Carrot::ShaderModule>>>& getModuleMap() const;

    public: // hot-reload support
        [[nodiscard]] bool shouldReload() const;
        void reload();
    };

}
