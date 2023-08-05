//
// Created by jglrxavpok on 03/08/2023.
//

#pragma once

#include <glm/glm.hpp>

namespace Carrot::Math {
    class Sphere;

    class AABB {
    public:
        glm::vec3 min {0.0f};
        glm::vec3 max {0.0f};

        AABB() = default;
        AABB(glm::vec3 minBounds, glm::vec3 maxBounds);

        AABB& loadFromSphere(const Math::Sphere& sphere);

        void computeCenterAndHalfSize(glm::vec3& center, glm::vec3& halfSize);
        bool contains(const glm::vec3& p);
    };

} // Carrot::Math
