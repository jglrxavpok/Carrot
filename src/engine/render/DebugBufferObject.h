//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include "engine/vulkan/includes.h"
#include <glm/glm.hpp>

namespace Carrot {
    enum class SingleGChannel: int32_t {
        All = -1,
        Albedo = 0,
        Position,
        Normals,
        Depth,
        RaytracingOnly,
        OnlyUI, // < UI is always shown
        Skybox,
        IntProperties,
    };

    struct DebugBufferObject {
        alignas(16) SingleGChannel gChannel = SingleGChannel::All;
    };
}
