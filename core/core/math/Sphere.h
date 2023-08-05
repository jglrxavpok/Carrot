//
// Created by jglrxavpok on 17/04/2023.
//

#pragma once

#include <glm/glm.hpp>

namespace Carrot::Math {
    class AABB;

    class Sphere {
    public:
        glm::vec3 center = glm::vec3{0.0f};
        float radius = 0.0f;

        Sphere& transform(const glm::mat4& transform);
        Sphere& loadFromAABB(const glm::vec3& minBB, const glm::vec3& maxBB);
        Sphere& loadFromAABB(const Math::AABB& bb);
    };
} // Carrot::Math
