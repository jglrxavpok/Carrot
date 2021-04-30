//
// Created by jglrxavpok on 26/11/2020.
//

#include "Vertex.h"
#include "engine/render/InstanceData.h"
#include "engine/render/particles/Particles.h"

std::vector<vk::VertexInputAttributeDescription> Carrot::Vertex::getAttributeDescriptions() {
    std::vector<vk::VertexInputAttributeDescription> descriptions{9};

    descriptions[0] = {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32A32Sfloat,
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
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(Vertex, normal)),
    };

    descriptions[3] = {
            .location = 3,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(Vertex, uv)),
    };

    descriptions[4] = {
            .location = 4,
            .binding = 1,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(InstanceData, color)),
    };

    for (int i = 0; i < 4; ++i) {
        descriptions[5+i] = {
                .location = static_cast<uint32_t>(5+i),
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

std::vector<vk::VertexInputAttributeDescription> Carrot::ComputeSkinnedVertex::getAttributeDescriptions() {
    std::vector<vk::VertexInputAttributeDescription> descriptions{9};

    descriptions[0] = {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32A32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(ComputeSkinnedVertex, pos)),
    };

    descriptions[1] = {
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(ComputeSkinnedVertex, color)),
    };

    descriptions[2] = {
            .location = 2,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(ComputeSkinnedVertex, normal)),
    };

    descriptions[3] = {
            .location = 3,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(ComputeSkinnedVertex, uv)),
    };

    descriptions[4] = {
            .location = 4,
            .binding = 1,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, color)),
    };

    for (int i = 0; i < 4; ++i) {
        descriptions[5+i] = {
                .location = static_cast<uint32_t>(5+i),
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, transform)+sizeof(glm::vec4)*i),
        };
    }

    return descriptions;
}

std::vector<vk::VertexInputBindingDescription> Carrot::ComputeSkinnedVertex::getBindingDescription() {
    return {vk::VertexInputBindingDescription {
                    .binding = 0,
                    .stride = sizeof(ComputeSkinnedVertex),
                    .inputRate = vk::VertexInputRate::eVertex,
            },
            vk::VertexInputBindingDescription {
                    .binding = 1,
                    .stride = sizeof(AnimatedInstanceData),
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
    std::vector<vk::VertexInputAttributeDescription> descriptions{13};

    descriptions[0] = {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32A32Sfloat,
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
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, normal)),
    };

    descriptions[3] = {
            .location = 3,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, uv)),
    };

    descriptions[4] = {
            .location = 4,
            .binding = 0,
            .format = vk::Format::eR32G32B32A32Sint,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, boneIDs)),
    };

    descriptions[5] = {
            .location = 5,
            .binding = 0,
            .format = vk::Format::eR32G32B32A32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, boneWeights)),
    };

    descriptions[6] = {
            .location = 6,
            .binding = 1,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, color)),
    };

    for (int i = 0; i < 4; ++i) {
        descriptions[7+i] = {
                .location = static_cast<uint32_t>(7+i),
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, transform)+sizeof(glm::vec4)*i),
        };
    }

    descriptions[11] = {
            .location = 11,
            .binding = 1,
            .format = vk::Format::eR32Uint,
            .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, animationIndex)),
    };

    descriptions[12] = {
            .location = 12,
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

std::vector<vk::VertexInputAttributeDescription> Carrot::ScreenSpaceVertex::getAttributeDescriptions() {
    std::vector<vk::VertexInputAttributeDescription> descriptions{1};

    descriptions[0] = {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(ScreenSpaceVertex, pos)),
    };

    return descriptions;
}

std::vector<vk::VertexInputBindingDescription> Carrot::ScreenSpaceVertex::getBindingDescription() {
    return {
        vk::VertexInputBindingDescription {
            .binding = 0,
            .stride = sizeof(ScreenSpaceVertex),
            .inputRate = vk::VertexInputRate::eVertex,
        },
    };
}

std::vector<vk::VertexInputAttributeDescription> Carrot::SimpleVertex::getAttributeDescriptions() {
    std::vector<vk::VertexInputAttributeDescription> descriptions{1};

    descriptions[0] = {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(SimpleVertex, pos)),
    };

    return descriptions;
}

std::vector<vk::VertexInputBindingDescription> Carrot::SimpleVertex::getBindingDescription() {
    return {
            vk::VertexInputBindingDescription {
                    .binding = 0,
                    .stride = sizeof(SimpleVertex),
                    .inputRate = vk::VertexInputRate::eVertex,
            },
    };
}

std::vector<vk::VertexInputAttributeDescription> Carrot::Particle::getAttributeDescriptions() {
    return {};
}

std::vector<vk::VertexInputBindingDescription> Carrot::Particle::getBindingDescription() {
    return {};
}