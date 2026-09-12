//
// Created by jglrxavpok on 03/08/2023.
//

#include "AABB.h"
#include <core/math/Sphere.h>

namespace Carrot::Math {
    AABB::AABB(glm::vec3 minBounds, glm::vec3 maxBounds): min(minBounds), max(maxBounds) {}

    AABB& AABB::loadFromSphere(const Math::Sphere& sphere) {
        glm::vec3 r{sphere.radius};
        min = sphere.center - r;
        max = sphere.center + r;
        return *this;
    }

    void AABB::computeCenterAndHalfSize(glm::vec3& center, glm::vec3& halfSize) {
        center = (min + max) / 2.0f;
        halfSize = (max - min) / 2.0f;
    }

    bool AABB::contains(const glm::vec3& p) {
        return
            p.x >= min.x && p.x <= max.x
            && p.y >= min.y && p.y <= max.y
            && p.z >= min.z && p.z <= max.z;
    }

    AABB AABB::computeEncompassingBoxAfterTransform(const glm::mat4& transform) const {
        std::array<glm::vec3, 8> corners = {
            glm::vec3(min.x, min.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(min.x, max.y, max.z),

            glm::vec3(max.x, min.y, min.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(max.x, max.y, max.z),
        };

        glm::vec3 bbMin{INFINITY, INFINITY, INFINITY};
        glm::vec3 bbMax{-INFINITY, -INFINITY, -INFINITY};
        for (const glm::vec3& v : corners) {
            const glm::vec3 transformed = (transform * glm::vec4(v, 1)).xyz();
            bbMin = glm::min(bbMin, transformed);
            bbMax = glm::max(bbMax, transformed);
        }

        return AABB{bbMin, bbMax};
    }
} // Carrot::Math