//
// Created by jglrxavpok on 26/11/2020.
//

#include "Vertex.h"
#include "InstanceData.h"

std::vector<vk::VertexInputAttributeDescription> Carrot::Vertex::getAttributeDescriptions() {
    std::vector<vk::VertexInputAttributeDescription> descriptions{8};

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

std::vector<vk::VertexInputBindingDescription> Carrot::Vertex::getBindingDescription() {
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

std::vector<vk::VertexInputBindingDescription> Carrot::SkinnedVertex::getBindingDescription() {
    return {vk::VertexInputBindingDescription {
            .binding = 0,
            .stride = sizeof(SkinnedVertex),
            .inputRate = vk::VertexInputRate::eVertex,
            },
            vk::VertexInputBindingDescription {
                    .binding = 1,
                    .stride = sizeof(AnimatedInstanceData),
                    .inputRate = vk::VertexInputRate::eInstance,
            },
    };
}

std::vector<vk::VertexInputAttributeDescription> Carrot::SkinnedVertex::getAttributeDescriptions() {
    std::vector<vk::VertexInputAttributeDescription> descriptions{12};

    descriptions[0] = {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, pos)),
    };

    descriptions[1] = {
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, color)),
    };

    descriptions[2] = {
            .location = 2,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, uv)),
    };

    descriptions[3] = {
            .location = 3,
            .binding = 0,
            .format = vk::Format::eR32G32B32A32Sint,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, boneIDs)),
    };

    descriptions[4] = {
            .location = 4,
            .binding = 0,
            .format = vk::Format::eR32G32B32A32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, boneWeights)),
    };

    descriptions[5] = {
            .location = 5,
            .binding = 1,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, color)),
    };

    for (int i = 0; i < 4; ++i) {
        descriptions[6+i] = {
                .location = static_cast<uint32_t>(6+i),
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, transform)+sizeof(glm::vec4)*i),
        };
    }

    descriptions[10] = {
            .location = 10,
            .binding = 1,
            .format = vk::Format::eR32Uint,
            .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, animationIndex)),
    };

    descriptions[11] = {
            .location = 11,
            .binding = 1,
            .format = vk::Format::eR32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, animationTime)),
    };
    return descriptions;
}

void Carrot::SkinnedVertex::addBoneInformation(uint32_t boneID, float weight) {
    for(size_t i = 0; i < 4; i++) {
        if(boneWeights[i] == 0.0f) {
            boneWeights[i] = weight;
            boneIDs[i] = boneID;
            return;
        }
    }

    assert(0);
}
