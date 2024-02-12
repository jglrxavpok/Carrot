//
// Created by jglrxavpok on 19/11/2023.
//

#pragma once

#include <cstdint>
#include <limits>
#include <core/math/Sphere.h>
#include <glm/vec3.hpp>

namespace Carrot::Render {
    struct Meshlet {
        std::uint32_t vertexOffset = 0;
        std::uint32_t vertexCount = 0;
        std::uint32_t indexOffset = 0;
        std::uint32_t indexCount = 0;

        std::uint32_t lod = 0;
        Math::Sphere boundingSphere;
        Math::Sphere parentBoundingSphere;

        // Meshlets form a graph where the root is the most simplified version of the entire model, and each node's children
        // are the meshlets which were simplified to create that node.
        // Everything is expressed in mesh space here
        float parentError = std::numeric_limits<float>::infinity(); // set to infinity if there is no parent (ie the node has no further simplification)
        float clusterError = 0.0f;
    };
}