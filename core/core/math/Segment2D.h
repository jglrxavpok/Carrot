//
// Created by jglrxavpok on 20/09/2022.
//

#pragma once

#include <glm/glm.hpp>

namespace Carrot::Math {
    struct Segment2D {
        glm::vec2 first{0.0f};
        glm::vec2 second{0.0f};

        Segment2D() = default;
        Segment2D(const glm::vec2& a, const glm::vec2& b);
        Segment2D(const Segment2D& toCopy) = default;
        Segment2D(Segment2D&& toMove) = default;

        Segment2D& operator=(const Segment2D& toCopy) = default;
        Segment2D& operator=(Segment2D&& toMove) = default;

        bool computeIntersection(const Segment2D& other, glm::vec2& outIntersectionPoint) const;

        /// <0 if p on left, >1 if p on right, 0 if p is on line
        float getSignedDistance(const glm::vec2& p) const;
    };
}
