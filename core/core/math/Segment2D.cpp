//
// Created by jglrxavpok on 20/09/2022.
//

#include "Segment2D.h"

namespace Carrot::Math {
    bool Segment2D::computeIntersection(const Segment2D& other, glm::vec2& outIntersectionPoint) const {
        float x1 = first.x;
        float y1 = first.y;
        float x2 = second.x;
        float y2 = second.y;

        float x3 = other.first.x;
        float y3 = other.first.y;
        float x4 = other.second.x;
        float y4 = other.second.y;

        float x21 = x2 - x1;
        float y21 = y2 - y1;
        float x43 = x4 - x3;
        float y43 = y4 - y3;
        float dot = x21 * x43 + y21 * y43;

        // parallel
        if(dot < 10e-12 && dot > -(10e12)) {
            // return false;
        }

        float x0Numerator = (y3*x4 - y4*x3) / x43 - (y1*x2 - y2*x1) / x21;
        float x0Denominator = y21 / x21 - y43 / x43;

        if(x0Denominator == 0.0f) {
            return false;
        }

        float x0 = x0Numerator / x0Denominator;
        float y0 = y21/x21 * x0 + (y1*x2-y2*x1) / x21;

        float minXMe = glm::min(first.x, second.x);
        float maxXMe = glm::max(first.x, second.x);

        float minXOther = glm::min(other.first.x, other.second.x);
        float maxXOther = glm::max(other.first.x, other.second.x);

        float minYMe = glm::min(first.y, second.y);
        float maxYMe = glm::max(first.y, second.y);

        float minYOther = glm::min(other.first.y, other.second.y);
        float maxYOther = glm::max(other.first.y, other.second.y);

        bool intersection = x0 > minXMe && x0 < maxXMe
                            && x0 > minXOther && x0 < maxXOther
                            && y0 > minYMe && y0 < maxYMe
                            && y0 > minYOther && y0 < maxYOther;

        if(intersection) {
            outIntersectionPoint = { x0, y0 };
            return true;
        }
        return false;
    }

    float Segment2D::getSignedDistance(const glm::vec2& p) const {
        glm::vec2 dir = second - first;
        return
            (dir.x * (first.y - p.y) - dir.y * (first.x - p.x))
            / glm::length(dir);
    }
}