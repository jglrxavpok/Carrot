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
#define DEBUG_GBUFFER_EMISSIVE 6
#define DEBUG_GBUFFER_RANDOMNESS 7
#define DEBUG_GBUFFER_TANGENT 8
#define DEBUG_GBUFFER_LIGHTING 9
#define DEBUG_GBUFFER_MOTION 10
#define DEBUG_GBUFFER_MOMENTS 11

namespace Carrot {

    struct DebugBufferObject {
        std::uint32_t gBufferType = DEBUG_GBUFFER_DISABLED;
    };
}
