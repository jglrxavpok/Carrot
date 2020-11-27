//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include "vulkan/includes.h"
#include <glm/glm.hpp>

namespace Carrot {
    struct UniformBufferObject {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model;
    };
}
