//
// Created by jglrxavpok on 17/04/2023.
//

#include "Plane.h"
#include "core/Macros.h"

namespace Carrot::Math {
    Plane Plane::fromTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        glm::vec3 normal = glm::cross(b - a, c - a);
        normal /= glm::length(normal);
        return Plane {
            .normal = normal,
            .distanceFromOrigin = glm::dot(normal, a)
        };
    }

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
        normal = abcd.xyz();
        distanceFromOrigin = abcd.w;
        return *this;
    }

    glm::vec3 Plane::project(const glm::vec3& v) {
        float distance = glm::dot(normal, v) - distanceFromOrigin;
        return v - distance * normal;
    }
} // Carrot::Math