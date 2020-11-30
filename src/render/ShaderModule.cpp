//
// Created by jglrxavpok on 30/11/2020.
//

#include "ShaderModule.h"
#include "io/IO.h"

Carrot::ShaderModule::ShaderModule(Carrot::Engine& engine, const string& filename, const string& entryPoint): engine(engine), entryPoint(entryPoint) {
    auto code = IO::readFile(filename);
    auto& device = engine.getLogicalDevice();
    vkModule = device.createShaderModuleUnique({
            .codeSize = static_cast<uint32_t>(code.size()),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    }, engine.getAllocator());
}

vk::PipelineShaderStageCreateInfo Carrot::ShaderModule::createPipelineShaderStage(vk::ShaderStageFlagBits stage) const {
    return {
        .stage = stage,
        .module = *vkModule,
        .pName = entryPoint.c_str(),
    };
}
