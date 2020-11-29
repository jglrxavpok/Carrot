//
// Created by jglrxavpok on 26/11/2020.
//

#include "Vertex.h"

std::array<vk::VertexInputAttributeDescription, 3> Carrot::Vertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 3> descriptions{};

    descriptions[0] = {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(Vertex, pos)),
    };

    descriptions[1] = {
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(Vertex, color)),
    };

    descriptions[2] = {
            .location = 2,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(Vertex, uv)),
    };

    return descriptions;
}

vk::VertexInputBindingDescription Carrot::Vertex::getBindingDescription() {
    return {
            .binding = 0, // TODO: custom index
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex, // TODO: change for instanced rendering
    };
}
