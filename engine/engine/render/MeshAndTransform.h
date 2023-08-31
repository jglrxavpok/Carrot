//
// Created by jglrxavpok on 30/08/2023.
//

#pragma once

#include <cstdint>
#include <core/math/Sphere.h>
#include <engine/render/resources/Mesh.h>

namespace Carrot {
    struct MeshAndTransform {
        std::shared_ptr<Mesh> mesh;
        glm::mat4 transform{1.0f};
        Math::Sphere boundingSphere;

        std::size_t meshIndex = 0;
        std::size_t staticMeshIndex = 0;
    };
}