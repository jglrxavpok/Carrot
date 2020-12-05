//
// Created by jglrxavpok on 26/11/2020.
//

#include "Vertex.h"
#include "InstanceData.h"

std::array<vk::VertexInputAttributeDescription, 8> Carrot::Vertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 8> descriptions{};

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

    descriptions[3] = {
            .location = 3,
            .binding = 1,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(InstanceData, color)),
    };

    for (int i = 0; i < 4; ++i) {
        descriptions[4+i] = {
                .location = static_cast<uint32_t>(4+i),
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(InstanceData, transform)+sizeof(glm::vec4)*i),
        };
    }

    return descriptions;
}

std::array<vk::VertexInputBindingDescription, 2> Carrot::Vertex::getBindingDescription() {
    return {vk::VertexInputBindingDescription {
                    .binding = 0,
                    .stride = sizeof(Vertex),
                    .inputRate = vk::VertexInputRate::eVertex,
            },
            vk::VertexInputBindingDescription {
                    .binding = 1,
                    .stride = sizeof(InstanceData),
                    .inputRate = vk::VertexInputRate::eInstance,
            },
    };
}
