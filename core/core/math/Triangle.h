//
// Created by jglrxavpok on 29/07/2023.
//

#pragma once

#include <glm/glm.hpp>

namespace Carrot::Math {

    class Triangle {
    public:
        glm::vec3 a { 0.0f };
        glm::vec3 b { 0.0f };
        glm::vec3 c { 0.0f };

        /**
         * Gets A, B or C depending on index (in this order)
         */
        glm::vec3& getPoint(u8 index);
        /**
         * Gets A, B or C depending on index (in this order)
         */
        const glm::vec3& getPoint(u8 index) const;

        [[nodiscard]] glm::vec3 getClosestPoint(const glm::vec3& p) const;
        [[nodiscard]] glm::vec3 getClosestPointOnEdges(const glm::vec3& p) const;
        bool isPointInside(const glm::vec3& p) const;

        [[nodiscard]] glm::vec3 computeNormal() const;
    };

} // Carrot::Math
