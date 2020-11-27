//
// Created by jglrxavpok on 26/11/2020.
//

#pragma once

#include <glm/glm.hpp>
#include "vulkan/includes.h"

namespace Carrot {
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;

        static vk::VertexInputBindingDescription getBindingDescription();

        static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions();
    };
}


