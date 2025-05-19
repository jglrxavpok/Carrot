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
}

#ifdef _MSC_VER
#define VK_DISPATCHER_TYPE vk::DispatchLoaderDynamic
#define VK_LOADER_TYPE vk::DynamicLoader
#else
#define VK_DISPATCHER_TYPE vk::detail::DispatchLoaderDynamic
#define VK_LOADER_TYPE vk::detail::DynamicLoader
#endif