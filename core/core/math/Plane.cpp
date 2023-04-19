//
// Created by jglrxavpok on 17/04/2023.
//

#include "Plane.h"
#include "core/Macros.h"

namespace Carrot::Math {
    float Plane::getSignedDistance(const glm::vec3& point) const {
        return glm::dot(normal, point) + distanceFromOrigin;
    }

    Plane& Plane::normalize() {
        float l = glm::length(normal);

        normal /= l;
        distanceFromOrigin /= l;
        return *this;
    }

    Plane& Plane::operator=(const glm::vec4& abcd) {
        normal = abcd.xyz;
        distanceFromOrigin = abcd.w;
        return *this;
    }
} // Carrot::Math