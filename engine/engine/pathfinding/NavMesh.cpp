//
// Created by jglrxavpok on 29/07/2023.
//

#include "NavMesh.h"
#include <core/Macros.h>
#include <core/math/Segment2D.h>
#include <core/math/Triangle.h>
#include <core/scene/LoadedScene.h>
#include <core/scene/GLTFLoader.h>
#include <glm/gtx/closest_point.hpp>
#include <core/utils/Profiling.h>

namespace Carrot::AI {

    NavMesh::NavMesh() {

    }

    // TODO: "CPU"-Mesh constructor (or something like that)

    void NavMesh::loadFromResource(const Carrot::IO::Resource& resource) {
        Render::GLTFLoader loader;
        loadFromScene(loader.load(resource));
    }

    void NavMesh::loadFromScene(const Render::LoadedScene& scene)  {
        // TODO: maybe //-ize if needed
        std::vector<NavMeshTriangle> triangles;

        std::vector<glm::vec3> allVertices;
        std::unordered_map<std::size_t, std::vector<std::size_t>> vertexToTriangles; // all triangles sharing a given vertex
        for(const auto& primitive : scene.primitives) {
            std::size_t indexCount = primitive.indices.size();

            std::size_t vertexIndexOffset = triangles.size() * 3;
            triangles.reserve(triangles.size() + indexCount / 3);

            allVertices.reserve(allVertices.size() + primitive.vertices.size());
            for(const auto& v : primitive.vertices) {
                allVertices.emplace_back(v.pos.xyz);
            }

            for (std::size_t i = 0; i < indexCount; i += 3) {
                std::size_t index0 = primitive.indices[i + 0];
                std::size_t index1 = primitive.indices[i + 1];
                std::size_t index2 = primitive.indices[i + 2];

                NavMeshTriangle& newTriangle = triangles.emplace_back();
                newTriangle.index = triangles.size()-1;
                newTriangle.triangle.a = primitive.vertices[index0].pos.xyz;
                newTriangle.triangle.b = primitive.vertices[index1].pos.xyz;
                newTriangle.triangle.c = primitive.vertices[index2].pos.xyz;

                newTriangle.globalVertexIndices[0] = index0 + vertexIndexOffset;
                newTriangle.globalVertexIndices[1] = index1 + vertexIndexOffset;
                newTriangle.globalVertexIndices[2] = index2 + vertexIndexOffset;

                newTriangle.center = (newTriangle.triangle.a + newTriangle.triangle.b + newTriangle.triangle.c) / 3.0f;

                vertexToTriangles[newTriangle.globalVertexIndices[0]].push_back(newTriangle.index);
                vertexToTriangles[newTriangle.globalVertexIndices[1]].push_back(newTriangle.index);
                vertexToTriangles[newTriangle.globalVertexIndices[2]].push_back(newTriangle.index);
            }
        }

        // triangle index -> connected triangles
        std::unordered_map<std::size_t, std::vector<std::size_t>> adjacency;

        // triangle -> other triangle -> shared vertices
        std::vector<std::unordered_map<std::size_t, std::vector<glm::vec3>>> allSharedVertices;
        allSharedVertices.resize(triangles.size());
        for(const auto& triangle : triangles) {
            auto& connected = adjacency[triangle.index];
            auto& sharedVertices = allSharedVertices[triangle.index];

            for (std::size_t vertexIndex : triangle.globalVertexIndices) {
                for(const auto& triangleIndex : vertexToTriangles[vertexIndex]) {
                    if(triangleIndex != triangle.index) {
                        sharedVertices[triangleIndex].push_back(allVertices[vertexIndex]);
                    }
                }
            }

            for(const auto& [otherTriangle, sharedVertexList] : sharedVertices) {
                if(sharedVertexList.size() >= 2) {
                    connected.push_back(otherTriangle);
                    auto& portal = portalVertices[triangle.index][otherTriangle];
                    portal[0] = sharedVertexList[0];
                    portal[1] = sharedVertexList[1];
                }
            }
        }

        std::vector<Edge> flatEdges;
        for(const auto& [triangleIndex, connectedTriangles] : adjacency) {
            flatEdges.reserve(flatEdges.size() + connectedTriangles.size());

            for(const auto& otherTriangle : connectedTriangles) {
                assert(otherTriangle != triangleIndex);
                Edge& newEdge = flatEdges.emplace_back();
                newEdge.indexA = triangleIndex;
                newEdge.indexB = otherTriangle;
            }
        }

        pathfinder.setGraph(std::move(triangles), std::move(flatEdges));
    }

    glm::vec3 NavMesh::getClosestPointInMesh(const glm::vec3& position) {
        return getClosestPosition(position).position;
    }

    NavPath NavMesh::computePath(const glm::vec3& pointA, const glm::vec3& pointB) {
        Profiling::PrintingScopedTimer _t("NavMesh::computePath");

        NavMeshPosition posA = getClosestPosition(pointA);
        NavMeshPosition posB = getClosestPosition(pointB);

        if(posA.triangleIndex == posB.triangleIndex) {
            return NavPath {
                .waypoints = {
                    pointA, pointB
                }
            };
        }

        // 1. find triangles to go through, via A*
        // cost estimate: distance between triangle centers
        auto distance = [&](const NavMeshTriangle& a, const NavMeshTriangle& b) {
            return glm::distance(a.center, b.center);
        };
        auto estimation = [&](const NavMeshTriangle& v) {
            return glm::distance(v.center, pointB);
        };
        std::vector<std::size_t> triangles = pathfinder.findPath(posA.triangleIndex, posB.triangleIndex, distance, estimation);

        // no path found
        if(triangles.empty()) {
            return NavPath{};
        }

        NavPath path;
        verify(triangles[0] == posA.triangleIndex, "Path does not start at point A ?");

        path.waypoints.push_back(pointA);
        funnel(posA, posB, triangles, path.waypoints);
        path.waypoints.push_back(pointB);

        return path;
    }

    // Comment from http://jceipek.com/Olin-Coding-Tutorials/pathing.html#funnel-algorithm :
    //  This function from http://digestingduck.blogspot.com/2010/03/simple-stupid-funnel-algorithm.html
    //  is called "triarea2" because the cross product of two vectors
    //  returns the area of the parallellogram the two sides form
    //  (which has twice the area of the triangle they form)
    //  The sign of the result will tell you the order of the points passed in.

    // also ignore Z component
    static float triangleArea2(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        const float ax = b[0] - a[0];
        const float ay = b[1] - a[1];
        const float bx = c[0] - a[0];
        const float by = c[1] - a[1];
        return bx*ay - ax*by;
    }

    static bool almostEqual(const glm::vec2& a, const glm::vec2& b) {
        return glm::dot(a-b, a-b) < 10e-12f;
    }

    void NavMesh::funnel(const NavMeshPosition& startPos, const NavMeshPosition& endPos, std::span<const std::size_t> triangles, std::vector<glm::vec3>& waypoints) {
        struct Portal {
            glm::vec3 left;
            glm::vec3 right;
        };

        std::vector<Portal> portals;
        portals.reserve(triangles.size() + 1);

        auto& startPortal = portals.emplace_back();
        startPortal.left = startPos.position;
        startPortal.right = startPos.position;

        // from list of triangles, create portals (with left and right relative to order in which triangles are traversed)
        for(std::size_t i = 0; i < triangles.size() - 1; i++) {
            std::size_t triangleIndexA = triangles[i];
            std::size_t triangleIndexB = triangles[i + 1];

            const NavMeshTriangle& triangleA = pathfinder.getVertices()[triangleIndexA];
            const NavMeshTriangle& triangleB = pathfinder.getVertices()[triangleIndexB];

            // assume ZUp + entities don't move 100% vertically
            const auto& vertices = portalVertices.at(triangleIndexA).at(triangleIndexB);
            Math::Segment2D segment;
            segment.first = triangleA.center.xy;
            segment.second = triangleB.center.xy;

            auto& portal = portals.emplace_back();
            if(segment.getSignedDistance(vertices[0]) < 0) {
                portal.left = vertices[0];
                portal.right = vertices[1];
            } else {
                portal.left = vertices[1];
                portal.right = vertices[0];
            }
        }

        auto& endPortal = portals.emplace_back();
        endPortal.left = endPos.position;
        endPortal.right = endPos.position;

        // funnel/string pulling (stupid simple funnel algorithm) http://digestingduck.blogspot.com/2010/03/simple-stupid-funnel-algorithm.html
        std::size_t apexIndex = 0;
        std::size_t leftIndex = 0;
        std::size_t rightIndex = 0;

        glm::vec3 portalApex = portals[0].left;
        glm::vec3 portalLeft = portals[0].left;
        glm::vec3 portalRight = portals[0].right;

        waypoints.push_back(portalApex);
        for (std::size_t i = 1; i < portals.size() && waypoints.size() < triangles.size(); ++i) {
            const glm::vec3& left = portals[i].left;
            const glm::vec3& right = portals[i].right;

            if(triangleArea2(portalApex, portalRight, right) <= 0.0f) { // right vertex is closer
                if(almostEqual(portalApex, portalRight) || triangleArea2(portalApex, portalLeft, right) > 0.0f) {
                    // Tighten
                    portalRight = right;
                    rightIndex = i;
                } else { // right vertex crossed over the left, insert new waypoint and restart
                    if(!almostEqual(portalLeft, waypoints.back())) {
                        waypoints.push_back(portalLeft);
                    }

                    portalApex = portalLeft;
                    apexIndex = leftIndex;

                    portalLeft = portalApex;
                    portalRight = portalApex;
                    leftIndex = apexIndex;
                    rightIndex = apexIndex;

                    // restart at new point
                    i = apexIndex;
                    continue;
                }
            }

            if(triangleArea2(portalApex, portalLeft, left) >= 0.0f) { // left vertex is closer
                if(almostEqual(portalApex, portalLeft) || triangleArea2(portalApex, portalRight, left) < 0.0f) {
                    // Tighten
                    portalLeft = left;
                    leftIndex = i;
                } else { // left vertex crossed over the right, insert new waypoint and restart
                    if(!almostEqual(portalRight, waypoints.back())) {
                        waypoints.push_back(portalRight);
                    }

                    portalApex = portalRight;
                    apexIndex = rightIndex;

                    portalLeft = portalApex;
                    portalRight = portalApex;
                    leftIndex = apexIndex;
                    rightIndex = apexIndex;

                    // restart at new point
                    i = apexIndex;
                    continue;
                }
            }
        }
    }

    NavMesh::NavMeshPosition NavMesh::getClosestPosition(const glm::vec3& position) {
        float minSqDistance = std::numeric_limits<float>::infinity();
        glm::vec3 closest { NAN, NAN, NAN };
        std::size_t closestTriangleIndex = ~0ull;
        for(const auto& navTriangle : pathfinder.getVertices()) { // ouch, I hope I can find something better than iterating over all triangles in the future
            const Carrot::Math::Triangle& triangle = navTriangle.triangle;

            const glm::vec3 p = triangle.getClosestPoint(position);
            const glm::vec3 delta = p - position;
            const float sqDistance = glm::dot(delta, delta);
            if(sqDistance < minSqDistance) {
                minSqDistance = sqDistance;
                closest = p;
                closestTriangleIndex = navTriangle.index;
            }
        }

        return NavMeshPosition {
            .triangleIndex = closestTriangleIndex,
            .position = closest
        };
    }
} // Carrot::AI