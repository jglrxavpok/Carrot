//
// Created by jglrxavpok on 26/11/2020.
//

#pragma once

#include <glm/glm.hpp>

namespace Carrot {
    /// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    /// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    /// Keep in sync with shaders/includes/buffers.glsl !!
    /// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    /// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    /// Definition of a vertex for this engine
    struct Vertex {
        /// World position of the vertex
        alignas(16) glm::vec4 pos;

        /// RGB color
        alignas(16) glm::vec3 color;

        /// Vertex Normal
        alignas(16) glm::vec3 normal;

        /// Vertex tangent, W is the sign of the bitangent (B = tangent.w * cross(N, T) with N,T orthonormalized)
        alignas(16) glm::vec4 tangent;

        /// UV coordinates
        alignas(16) glm::vec2 uv;
    };

    struct ComputeSkinnedVertex {
        /// World position of the vertex
        alignas(16) glm::vec4 pos;

        /// RGB color
        alignas(16) glm::vec3 color;

        /// Vertex Normal
        alignas(16) glm::vec3 normal;

        /// UV coordinates
        alignas(16) glm::vec2 uv;
    };

    struct SkinnedVertex: public Vertex {
        /// Skinning information
        alignas(16) glm::ivec4 boneIDs{-1,-1,-1,-1};
        alignas(16) glm::vec4 boneWeights{0,0,0,0};

        void addBoneInformation(uint32_t boneID, float weight);
        void normalizeWeights();
    };

    struct ScreenSpaceVertex {
        /// Screen position of the vertex
        alignas(16) glm::vec2 pos;
    };

    struct SimpleVertex {
        /// Screen position of the vertex
        alignas(16) glm::vec3 pos;
    };

    struct SimpleVertexWithInstanceData {
        /// Screen position of the vertex
        alignas(16) glm::vec3 pos;
    };

}


