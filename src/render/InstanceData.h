//
// Created by jglrxavpok on 05/12/2020.
//

#pragma once
#include <glm/glm.hpp>

namespace Carrot {
    struct InstanceData {
        alignas(16) glm::vec4 color;
        alignas(16) glm::mat4 transform;
    };

    struct AnimatedInstanceData {
        alignas(16) glm::vec4 color;
        alignas(16) glm::mat4 transform;
        alignas(16) uint32_t animationIndex;
        float animationTime;
    };
}

