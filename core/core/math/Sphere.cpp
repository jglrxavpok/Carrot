//
// Created by jglrxavpok on 17/04/2023.
//

#include "Sphere.h"
#include "core/Macros.h"
#include "glm/gtx/norm.hpp"

namespace Carrot::Math {

    Sphere& Sphere::transform(const glm::mat4& transform) {
        glm::vec4 hCenter = glm::vec4(center, 1.0f);
        hCenter = transform * hCenter;
        center.x = hCenter.x / hCenter.w;
        center.y = hCenter.y / hCenter.w;
        center.z = hCenter.z / hCenter.w;

        glm::vec3 squaredScale {
            glm::length2(glm::vec3(transform[0])),
            glm::length2(glm::vec3(transform[1])),
            glm::length2(glm::vec3(transform[2])),
        };

        float scaleFactor = std::sqrt(glm::compMax(squaredScale));
        radius *= scaleFactor;

        return *this;
    }

    Sphere& Sphere::loadFromAABB(const glm::vec3& minBB, const glm::vec3& maxBB) {
        center = (maxBB + minBB) / 2.0f;
        radius = glm::distance(maxBB, minBB) / 2.0f;
        return *this;
    }
} // Carrot::Math