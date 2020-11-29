//
// Created by jglrxavpok on 26/11/2020.
//

#pragma once

#include <glm/glm.hpp>
#include "vulkan/includes.h"

namespace Carrot {
    struct Vertex {
        alignas(16) glm::vec3 pos;
        alignas(16) glm::vec3 color;
        alignas(16) glm::vec2 uv;

        static vk::VertexInputBindingDescription getBindingDescription();

        static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions();
    };
}


