//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once
#include <string>
#include "vulkan/includes.h"
#include "Engine.h"

using namespace std;

namespace Carrot {
    class ShaderModule {
    private:
        Carrot::Engine& engine;
        vk::UniqueShaderModule vkModule{};
        string entryPoint;

    public:
        explicit ShaderModule(Carrot::Engine& engine, const string& filename, const string& entryPoint = "main");

        [[nodiscard]] vk::PipelineShaderStageCreateInfo createPipelineShaderStage(vk::ShaderStageFlagBits stage) const;
    };
}
