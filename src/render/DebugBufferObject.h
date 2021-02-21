//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include "vulkan/includes.h"
#include <glm/glm.hpp>

namespace Carrot {
    struct DebugBufferObject {
        alignas(16) bool onlyRaytracing = false;
    };
}
