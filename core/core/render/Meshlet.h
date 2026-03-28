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
        std::uint32_t groupIndex = 0; // used to group meshlets together for raytracing optimisations.

        std::uint32_t lod = 0;
        Math::Sphere boundingSphere;
        Math::Sphere refinedBoundingSphere;

        // Everything is expressed in mesh space here
        float refinedError = 0.0f; // error of the group that was used to generate this meshlet. 0 if it is from the original mesh
        float clusterError = std::numeric_limits<float>::infinity();
    };
}