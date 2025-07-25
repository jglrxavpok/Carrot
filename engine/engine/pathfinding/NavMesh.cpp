//
// Created by jglrxavpok on 29/07/2023.
//

#include "NavMesh.h"
#include <core/Macros.h>
#include <core/math/Segment2D.h>
#include <core/math/Triangle.h>
#include <core/scene/LoadedScene.h>
#include <glm/gtx/closest_point.hpp>
#include <core/utils/Profiling.h>
#include <engine/render/resources/model_loading/SceneLoader.h>
#include <core/io/FileFormats.h>
#include <core/io/Serialisation.h>
#include <engine/render/RenderContext.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/utils/Macros.h>
#include <glm/gtx/vector_query.hpp>

static constexpr std::array<char, 4> CNAVMagic = { 'C', 'N', 'A', 'V' };
static constexpr std::uint32_t CNAVVersion = 0;

namespace Carrot::AI {

    NavMesh::NavMesh() {

    }

    void NavMesh::loadFromResource(const Carrot::IO::Resource& resource) {
        const char* resourceName = resource.getName().c_str();
        if(IO::getFileFormat(resourceName) == IO::FileFormat::CNAV) {
            std::vector<std::uint8_t> data;
            data.resize(resource.getSize());
            resource.read(data);

            IO::VectorReader reader { data };

            // write header
            std::array<char, 4> magic { '\0', '\0', '\0', '\0' };
            reader >> std::span(magic);
            if(magic != CNAVMagic) {
                throw std::invalid_argument(Carrot::sprintf("[NavMesh] File %s does not have the proper magic header", resourceName));
            }

            std::uint32_t version;
            reader >> version;
            if(version != CNAVVersion) {
                throw std::invalid_argument(Carrot::sprintf("[NavMesh] File %s does not a valid version", resourceName));
            }

            // write triangles
            std::vector<NavMeshTriangle> triangles;
            reader >> triangles;

            // write adjacency
            std::vector<Edge> edges;
            reader >> edges;

            reader >> portalVertices;
            pathfinder.setGraph(std::move(triangles), std::move(edges));
        } else {
            Render::SceneLoader loader;
            loadFromScene(loader.load(resource));
        }
    }

    void NavMesh::loadFromScene(const Render::LoadedScene& scene)  {
        // TODO: maybe //-ize if needed
        std::vector<NavMeshTriangle> triangles;

        std::vector<glm::vec3> allVertices;
        for(const auto& primitive : scene.primitives) {
            std::size_t indexCount = primitive.indices.size();

            std::size_t vertexIndexOffset = triangles.size() * 3;
            triangles.reserve(triangles.size() + indexCount / 3);

            allVertices.reserve(allVertices.size() + primitive.vertices.size());
            for(const auto& v : primitive.vertices) {
                allVertices.emplace_back(v.pos.xyz());
            }

            for (std::size_t i = 0; i < indexCount; i += 3) {
                std::size_t index0 = primitive.indices[i + 0];
                std::size_t index1 = primitive.indices[i + 1];
                std::size_t index2 = primitive.indices[i + 2];

                NavMeshTriangle& newTriangle = triangles.emplace_back();
                newTriangle.index = triangles.size()-1;
                newTriangle.triangle.a = primitive.vertices[index0].pos.xyz();
                newTriangle.triangle.b = primitive.vertices[index1].pos.xyz();
                newTriangle.triangle.c = primitive.vertices[index2].pos.xyz();

                newTriangle.globalVertexIndices[0] = index0 + vertexIndexOffset;
                newTriangle.globalVertexIndices[1] = index1 + vertexIndexOffset;
                newTriangle.globalVertexIndices[2] = index2 + vertexIndexOffset;

                newTriangle.center = (newTriangle.triangle.a + newTriangle.triangle.b + newTriangle.triangle.c) / 3.0f;
            }
        }

        // Due to the way regions are generated, it is possible that the triangles of two regions have overlapping edges,
        //  but no shared vertex (or a single one). Therefore we need to find all edges which are overlapping.

        // triangle index -> connected triangles
        std::unordered_map<std::size_t, std::vector<std::size_t>> adjacency;

        for(const auto& triangle1 : triangles) {
            for(const auto& triangle2 : triangles) { // TODO: no need to redo triangles with index < self
                if (triangle1.index == triangle2.index) {
                    continue;
                }

                // TODO: might be able to optimize this (using the bounding boxes of triangles?)

                // go over all edges of both triangles
                for (i32 vertex1 = 0; vertex1 < 3; vertex1++) {
                    for (i32 vertex2 = 0; vertex2 < 3; vertex2++) {
                        const glm::vec3& point1A = triangle1.triangle.getPoint((vertex1+0) % 3);
                        const glm::vec3& point1B = triangle1.triangle.getPoint((vertex1+1) % 3);
                        const glm::vec3& point2A = triangle2.triangle.getPoint((vertex2+0) % 3);
                        const glm::vec3& point2B = triangle2.triangle.getPoint((vertex2+1) % 3);

                        const glm::vec3 dir1 = point1B - point1A;
                        const glm::vec3 dir2 = point2B - point2A;

                        if (glm::areCollinear(dir1, dir2, 10e-6f)) {
                            const glm::vec3 normalizedDir1 = glm::normalize(dir1);
                            // project points of edge 2 on edge 1, and see if projection is very close to the actual point
                            auto proj = [&](const glm::vec3& v) {
                                return glm::dot((v - point1A), normalizedDir1);
                            };
                            const float tA = proj(point2A);
                            const float tB = proj(point2B);
                            const glm::vec3 point2AProjected = tA * normalizedDir1 + point1A;
                            const glm::vec3 point2BProjected = tB * normalizedDir1 + point1A;

                            if (glm::all(glm::epsilonEqual(point2A, point2AProjected, 10e-6f))
                                && glm::all(glm::epsilonEqual(point2B, point2BProjected, 10e-6f))) {
                                // if projections are veryyy close to the actual point, this means the two edges are aligned
                                // now we check that they intersect
                                // points A and B of triangle 1 are implicitly at t=0 and t=1

                                bool intersect = !(
                                    (tA < 0 && tB < 0) // completely to the 'left' of edge1
                                    || (tA > 1 && tB > 1) // completely to the 'right' of edge1
                                    );
                                if (intersect) {
                                    adjacency[triangle1.index].push_back(triangle2.index);
                                    // TODO: adjacency[triangle2.index].push_back(triangle1.index);

                                    auto& portal = portalVertices[triangle1.index][triangle2.index];

                                    // sort vertices along dir1, and take the #1 and #2 of the sorted list, we need to find the opening
                                    std::array<glm::vec3, 4> sortedAlongDir1{};
                                    sortedAlongDir1[0] = point1A;
                                    sortedAlongDir1[1] = point1B;
                                    sortedAlongDir1[2] = point2A;
                                    sortedAlongDir1[3] = point2B;

                                    std::ranges::sort(sortedAlongDir1, [&](const glm::vec3& a, const glm::vec3& b) {
                                        return proj(b) - proj(a) < 0;
                                    });

                                    portal[0] = sortedAlongDir1[1];
                                    portal[1] = sortedAlongDir1[2];
                                }
                            }
                        }
                    }
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

    IO::VectorReader& operator>>(IO::VectorReader& i, NavMesh::NavMeshTriangle& triangle) {
        i >> triangle.index;
        i >> triangle.triangle.a;
        i >> triangle.triangle.b;
        i >> triangle.triangle.c;

        triangle.center = (triangle.triangle.a + triangle.triangle.b + triangle.triangle.c) / 3.0f;

        return i;
    }

    IO::VectorReader& operator>>(IO::VectorReader& i, Edge& edge) {
        i >> edge.indexA;
        i >> edge.indexB;
        return i;
    }

    IO::VectorWriter& operator<<(IO::VectorWriter& o, const NavMesh::NavMeshTriangle& triangle) {
        o << triangle.index;
        o << triangle.triangle.a;
        o << triangle.triangle.b;
        o << triangle.triangle.c;
        return o;
    }

    IO::VectorWriter& operator<<(IO::VectorWriter& o, const Edge& edge) {
        o << edge.indexA;
        o << edge.indexB;
        return o;
    }

    void NavMesh::serialize(Carrot::IO::FileHandle& output) const {
        std::vector<std::uint8_t> data;
        IO::VectorWriter writer { data };

        // write header
        writer << std::span(CNAVMagic);
        writer << CNAVVersion;

        // write triangles
        std::vector<NavMeshTriangle> triangles;
        triangles.reserve(pathfinder.getVertices().size());
        for(const auto& t : pathfinder.getVertices()) {
            triangles.emplace_back(t);
        }
        writer << triangles;

        // write adjacency
        std::vector<Edge> edges;
        edges.reserve(pathfinder.getEdges().size());
        for(const auto& e : pathfinder.getEdges()) {
            edges.emplace_back(e);
        }
        writer << edges;

        writer << portalVertices;

        output.write(data);
    }

    bool NavMesh::hasTriangles() const {
        return !pathfinder.getVertices().empty();
    }

    void NavMesh::debugDraw() {
        const glm::vec4 color{ 0, 0, 0, 1 };
        Render::DebugRenderer& debugRenderer = GetRenderer().getDebugRenderer();
        const auto& vertices = pathfinder.getVertices();
        for (const auto& [indexA, indexB] : pathfinder.getEdges()) {
            const glm::vec3& a = vertices[indexA].center;
            const glm::vec3& b = vertices[indexB].center;

            debugRenderer.drawLine(a, b, color);
        }
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
            segment.first = triangleA.center.xy();
            segment.second = triangleB.center.xy();

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