//
// Created by jglrxavpok on 16/12/2022.
//

#include "VertexTypes.h"

constexpr int BoneWeightCount = 4;

Carrot::PackedVertex::PackedVertex(const Carrot::Vertex& v) {
    pos = v.pos;
    color = v.color;
    normal = v.normal;
    tangent = v.tangent;
    uv = v.uv;
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
