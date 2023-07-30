//
// Created by jglrxavpok on 29/07/2023.
//

#include "NavMesh.h"
#include <core/Macros.h>
#include <core/math/Triangle.h>
#include <core/scene/LoadedScene.h>
#include <core/scene/GLTFLoader.h>
#include <glm/gtx/closest_point.hpp>

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
        std::unordered_map<std::size_t, std::vector<std::size_t>> vertexToTriangles; // all triangles sharing a given vertex
        for(const auto& primitive : scene.primitives) {
            std::size_t indexCount = primitive.indices.size();

            std::size_t vertexIndexOffset = triangles.size() * 3;
            triangles.reserve(triangles.size() + indexCount / 3);

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
        std::unordered_map<std::size_t, std::size_t> sharedVertices;
        for(const auto& triangle : triangles) {
            auto& connected = adjacency[triangle.index];

            sharedVertices.clear();

            for (std::size_t vertexIndex : triangle.globalVertexIndices) {
                for(const auto& triangleIndex : vertexToTriangles[vertexIndex]) {
                    if(triangleIndex != triangle.index) {
                        sharedVertices[triangleIndex]++;
                    }
                }
            }

            for(const auto& [otherTriangle, sharedVertexCount] : sharedVertices) {
                if(sharedVertexCount >= 2) {
                    connected.push_back(otherTriangle);
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
        NavMeshPosition posA = getClosestPosition(pointA);
        NavMeshPosition posB = getClosestPosition(pointB);

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
        NavMeshPosition currentPosition = posA;
        verify(triangles[0] == currentPosition.triangleIndex, "Path does not start at point A ?");

        path.waypoints.push_back(pointA);
        path.waypoints.push_back(posA.position);

        // TODO: funnel/string pulling

        for (std::size_t index = 1; index < triangles.size(); ++index) {
            const glm::vec3& currentPositionIn3D = currentPosition.position;

            // 2. for each triangle, find closest point of next triangle to current position (closest point is on one of the edges of the triangle)
            const NavMeshTriangle& navTriangle = pathfinder.getVertices()[triangles[index]];
            const glm::vec3 closestPoint = navTriangle.triangle.getClosestPointOnEdges(currentPositionIn3D);

            // 3. move current position to closest point of next triangle
            currentPosition = NavMeshPosition {
                    .triangleIndex = navTriangle.index,
                    //.position = closestPoint
                    .position = navTriangle.center
            };
            path.waypoints.push_back(currentPosition.position);

            // 4. repeat until same triangle as posB
        }

        verify(currentPosition.triangleIndex == posB.triangleIndex, "Path was not completed?");

        // we may have a few units to move still (we reached the wanted triangle, but not the correct point yet)
        path.waypoints.push_back(posB.position);
        path.waypoints.push_back(pointB);

        return path;
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