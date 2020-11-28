//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include "vulkan/includes.h"
#include <glm/glm.hpp>

namespace Carrot {
    struct UniformBufferObject {
        alignas(16) glm::mat4 projection;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 model;
    };
}
