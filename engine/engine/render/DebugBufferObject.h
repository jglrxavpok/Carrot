//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include "engine/vulkan/includes.h"
#include <glm/glm.hpp>

#define DEBUG_GBUFFER_DISABLED 0
#define DEBUG_GBUFFER_ALBEDO 1
#define DEBUG_GBUFFER_POSITION 2
#define DEBUG_GBUFFER_DEPTH 3
#define DEBUG_GBUFFER_NORMAL 4
#define DEBUG_GBUFFER_METALLIC_ROUGHNESS 5

namespace Carrot {

    struct DebugBufferObject {
        std::uint32_t gBufferType = DEBUG_GBUFFER_DISABLED;
    };
}
