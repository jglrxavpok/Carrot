//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once
#include <vector>
#include <string>
#include "ShaderModule.h"
#include <map>
#include <core/containers/Vector.hpp>

namespace Carrot {
    class ShaderStages {
    private:
        std::vector<std::pair<vk::ShaderStageFlagBits, std::unique_ptr<Carrot::ShaderModule>>> stages{};

    public:
        explicit ShaderStages(const Carrot::Vector<Render::ShaderSource>& filenames, const Vector<vk::ShaderStageFlagBits>& stages, const Carrot::Vector<std::string>& entryPoints);

    public:
        [[nodiscard]] std::vector<vk::PipelineShaderStageCreateInfo> createPipelineShaderStages(const vk::SpecializationInfo* specialization = nullptr) const;

        [[nodiscard]] vk::UniqueDescriptorSetLayout createDescriptorSetLayout(std::uint32_t setID, const std::map<std::string, std::uint32_t>& constants) const;

        [[nodiscard]] const std::vector<std::pair<vk::ShaderStageFlagBits, std::unique_ptr<Carrot::ShaderModule>>>& getModuleMap() const;

    public: // hot-reload support
        [[nodiscard]] bool shouldReload() const;
        void reload();
    };

}
