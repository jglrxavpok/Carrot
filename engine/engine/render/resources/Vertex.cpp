//
// Created by jglrxavpok on 26/11/2020.
//

#include "Vertex.h"
#include "engine/render/InstanceData.h"
#include "engine/render/particles/Particles.h"

constexpr int BoneWeightCount = 4;

std::vector<vk::VertexInputAttributeDescription> Carrot::Vertex::getAttributeDescriptions() {
    std::vector<vk::VertexInputAttributeDescription> descriptions{15};

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
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(Vertex, tangent)),
    };

    descriptions[4] = {
            .location = 4,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(Vertex, uv)),
    };

    descriptions[5] = {
            .location = 5,
            .binding = 1,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(InstanceData, color)),
    };

    descriptions[6] = {
            .location = 6,
            .binding = 1,
            .format = vk::Format::eR32G32B32A32Uint,
            .offset = static_cast<uint32_t>(offsetof(InstanceData, uuid)),
    };

    for (int i = 0; i < 4; ++i) {
        descriptions[7+i] = {
                .location = static_cast<uint32_t>(7+i),
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(InstanceData, transform)+sizeof(glm::vec4)*i),
        };
    }

    for (int i = 0; i < 4; ++i) {
        descriptions[11+i] = {
                .location = static_cast<uint32_t>(11+i),
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(InstanceData, lastFrameTransform)+sizeof(glm::vec4)*i),
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
    std::vector<vk::VertexInputAttributeDescription> descriptions{14};

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

    descriptions[5] = {
            .location = 5,
            .binding = 1,
            .format = vk::Format::eR32G32B32A32Uint,
            .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, uuid)),
    };

    for (int i = 0; i < 4; ++i) {
        descriptions[6+i] = {
                .location = static_cast<uint32_t>(6+i),
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, transform)+sizeof(glm::vec4)*i),
        };
    }

    for (int i = 0; i < 4; ++i) {
        descriptions[10+i] = {
                .location = static_cast<uint32_t>(10+i),
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, lastFrameTransform)+sizeof(glm::vec4)*i),
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
    std::vector<vk::VertexInputAttributeDescription> descriptions{19};

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
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, tangent)),
    };

    descriptions[4] = {
            .location = 4,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, uv)),
    };

    descriptions[5] = {
            .location = 5,
            .binding = 0,
            .format = vk::Format::eR32G32B32A32Sint,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, boneIDs)),
    };

    descriptions[6] = {
            .location = 6,
            .binding = 0,
            .format = vk::Format::eR32G32B32A32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(SkinnedVertex, boneWeights)),
    };

    descriptions[7] = {
            .location = 7,
            .binding = 1,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, color)),
    };

    descriptions[8] = {
            .location = 8,
            .binding = 1,
            .format = vk::Format::eR32G32B32A32Uint,
            .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, uuid)),
    };

    for (int i = 0; i < 4; ++i) {
        descriptions[9+i] = {
                .location = static_cast<uint32_t>(9+i),
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, transform)+sizeof(glm::vec4)*i),
        };
    }

    for (int i = 0; i < 4; ++i) {
        descriptions[13+i] = {
                .location = static_cast<uint32_t>(13+i),
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, lastFrameTransform)+sizeof(glm::vec4)*i),
        };
    }

    descriptions[17] = {
            .location = 12,
            .binding = 1,
            .format = vk::Format::eR32Uint,
            .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, animationIndex)),
    };

    descriptions[18] = {
            .location = 13,
            .binding = 1,
            .format = vk::Format::eR32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(AnimatedInstanceData, animationTime)),
    };
    return descriptions;
}

void Carrot::SkinnedVertex::addBoneInformation(uint32_t boneID, float weight) {
    std::int32_t leastInfluential = -1;
    float smallestWeight = 10000.0f;

    for(size_t i = 0; i < BoneWeightCount; i++) {
        if(boneWeights[i] < weight) {
            if(smallestWeight > boneWeights[i]) {
                smallestWeight = boneWeights[i];
                leastInfluential = i;
            }
        }

        // free slot
        if(boneWeights[i] == 0.0f) {
            boneWeights[i] = weight;
            boneIDs[i] = boneID;
            return;
        }
    }

    if(leastInfluential < 0)
        return;

    // replace least influential
    boneWeights[leastInfluential] = weight;
    boneIDs[leastInfluential] = boneID;
}

void Carrot::SkinnedVertex::normalizeWeights() {
    float totalWeight = 0.0f;
    for (int j = 0; j < BoneWeightCount; ++j) {
        totalWeight += boneWeights[j];
    }

    if(totalWeight < 10e-6) {
        return;
    }

    for (int j = 0; j < BoneWeightCount; ++j) {
        boneWeights[j] /= totalWeight;
    }
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

std::vector<vk::VertexInputAttributeDescription> Carrot::SimpleVertexWithInstanceData::getAttributeDescriptions() {
    std::vector<vk::VertexInputAttributeDescription> descriptions{11};

    descriptions[0] = {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(SimpleVertexWithInstanceData, pos)),
    };

    descriptions[1] = {
            .location = 1,
            .binding = 1,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = static_cast<uint32_t>(offsetof(InstanceData, color)),
    };

    descriptions[2] = {
            .location = 2,
            .binding = 1,
            .format = vk::Format::eR32G32B32A32Uint,
            .offset = static_cast<uint32_t>(offsetof(InstanceData, uuid)),
    };

    for (int i = 0; i < 4; ++i) {
        descriptions[3+i] = {
                .location = static_cast<uint32_t>(3+i),
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(InstanceData, transform)+sizeof(glm::vec4)*i),
        };
    }

    for (int i = 0; i < 4; ++i) {
        descriptions[7+i] = {
                .location = static_cast<uint32_t>(7+i),
                .binding = 1,
                .format = vk::Format::eR32G32B32A32Sfloat,
                .offset = static_cast<uint32_t>(offsetof(InstanceData, lastFrameTransform)+sizeof(glm::vec4)*i),
        };
    }

    return descriptions;
}

std::vector<vk::VertexInputBindingDescription> Carrot::SimpleVertexWithInstanceData::getBindingDescription() {
    return {
            vk::VertexInputBindingDescription {
                    .binding = 0,
                    .stride = sizeof(SimpleVertexWithInstanceData),
                    .inputRate = vk::VertexInputRate::eVertex,
            },
            vk::VertexInputBindingDescription {
                    .binding = 1,
                    .stride = sizeof(InstanceData),
                    .inputRate = vk::VertexInputRate::eInstance,
            },
    };
}

std::vector<vk::VertexInputAttributeDescription> Carrot::Particle::getAttributeDescriptions() {
    return {};
}

std::vector<vk::VertexInputBindingDescription> Carrot::Particle::getBindingDescription() {
    return {};
}