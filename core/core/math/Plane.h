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

        float getSignedDistance(const glm::vec3& point) const;
        Plane& normalize();

        Plane& operator=(const glm::vec4& abcd);
    };
} // Carrot::Math
