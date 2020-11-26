//
// Created by jglrxavpok on 26/11/2020.
//

#pragma once

#include <glm/glm.hpp>
#include "vulkan/includes.h"

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription() {
        return {
                .binding = 0, // TODO: custom index
                .stride = sizeof(Vertex),
                .inputRate = vk::VertexInputRate::eVertex, // TODO: change for instanced rendering
        };
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 2> descriptions{};

        descriptions[0] = {
                .location = 0,
                .binding = 0,
                .format = vk::Format::eR32G32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(Vertex, pos)),
        };

        descriptions[1] = {
                .location = 1,
                .binding = 0,
                .format = vk::Format::eR32G32B32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(Vertex, color)),
        };

        return descriptions;
    }
};


