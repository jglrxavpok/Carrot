//
// Created by jglrxavpok on 20/02/2024.
//

#pragma once

namespace Carrot {
    constexpr vk::ShaderStageFlags AllVkStages = static_cast<vk::ShaderStageFlagBits>(0)
    | vk::ShaderStageFlagBits::eAllGraphics
    | vk::ShaderStageFlagBits::eCompute
    | vk::ShaderStageFlagBits::eRaygenKHR
    | vk::ShaderStageFlagBits::eMeshEXT
    | vk::ShaderStageFlagBits::eTaskEXT
    ;
};