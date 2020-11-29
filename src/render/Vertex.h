//
// Created by jglrxavpok on 26/11/2020.
//

#pragma once

#include <glm/glm.hpp>
#include "vulkan/includes.h"

namespace Carrot {
    /// Definition of a vertex for this engine
    struct Vertex {
        /// World position of the vertex
        alignas(16) glm::vec3 pos;

        /// RGB color
        alignas(16) glm::vec3 color;

        /// UV coordinates
        alignas(16) glm::vec2 uv;

        /// Creates a binding description for this Vertex struct
        static vk::VertexInputBindingDescription getBindingDescription();

        /// Creates an array describing the different attributes of a Vertex
        static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions();
    };
}


