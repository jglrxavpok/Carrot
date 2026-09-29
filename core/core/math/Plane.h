//
// Created by jglrxavpok on 17/04/2023.
//

#pragma once

#include <glm/glm.hpp>

namespace Carrot::Math {
    class Plane {
    public:
        glm::vec3 normal;
        float distanceFromOrigin;

        static Plane fromTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

        /// Requires plane to be normalized
        float getSignedDistance(const glm::vec3& point) const;
        Plane& normalize();

        Plane& operator=(const glm::vec4& abcd);

        glm::vec3 project(const glm::vec3& v);
    };

    static_assert(sizeof(Plane) == sizeof(glm::vec4), "Size of Math::Plane must match with vec4, intended to be sent as-is to the GPU");
} // Carrot::Math
