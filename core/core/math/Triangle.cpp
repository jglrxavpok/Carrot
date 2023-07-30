//
// Created by jglrxavpok on 29/07/2023.
//

#include "Triangle.h"
#include <core/Macros.h>
#include <core/math/Plane.h>
#include <glm/gtx/closest_point.hpp>

namespace Carrot::Math {

    glm::vec3 Triangle::getClosestPoint(const glm::vec3& p) const {
        Plane plane = Plane::fromTriangle(a, b, c);
        glm::vec3 projected = plane.project(p);

        if(isPointInside(projected)) {
            return projected;
        }

        return getClosestPointOnEdges(p);
    }

    glm::vec3 Triangle::getClosestPointOnEdges(const glm::vec3& p) const {
        const glm::vec3 closestOnEdge1 = glm::closestPointOnLine(p, a, b);
        const glm::vec3 closestOnEdge2 = glm::closestPointOnLine(p, b, c);
        const glm::vec3 closestOnEdge3 = glm::closestPointOnLine(p, c, a);

        auto sqDistanceToCurrent = [&](const glm::vec3& v) {
            glm::vec3 diff = v - p;
            return glm::dot(diff, diff);
        };
        glm::vec3 closestPoint = closestOnEdge1;
        if(sqDistanceToCurrent(closestOnEdge2) < sqDistanceToCurrent(closestPoint)) {
            closestPoint = closestOnEdge2;
        }
        if(sqDistanceToCurrent(closestOnEdge3) < sqDistanceToCurrent(closestPoint)) {
            closestPoint = closestOnEdge3;
        }

        return closestPoint;
    }

    bool Triangle::isPointInside(const glm::vec3& p) const {
        // based on https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/point_in_triangle.html
        const glm::vec3 offsetA = a - p;
        const glm::vec3 offsetB = b - p;
        const glm::vec3 offsetC = c - p;

        const glm::vec3 u = glm::cross(offsetB, offsetC);
        const glm::vec3 v = glm::cross(offsetC, offsetA);
        const glm::vec3 w = glm::cross(offsetA, offsetB);

        if(glm::dot(u, v) < 0.0f) {
            return false;
        }
        if(glm::dot(u, w) < 0.0f) {
            return false;
        }

        return true;
    }
} // Carrot::Math