//
// Created by jglrxavpok on 29/07/2023.
//

#pragma once

#include <engine/pathfinding/AStar.h>
#include <engine/pathfinding/NavPath.h>
#include <core/math/Triangle.h>
#include <core/scene/LoadedScene.h>
#include <core/io/Resource.h>
#include <core/io/Serialisation.h>

namespace Carrot::AI {

    /// Static navigation mesh. Can be used by AI agents to navigate between points inside a level
    class NavMesh {
    public:
        explicit NavMesh();

        /// expects glTF
        void loadFromResource(const Carrot::IO::Resource& resource);

        void loadFromScene(const Render::LoadedScene& scene);

        /// Finds the closest point to 'position' that is inside the mesh (not necessarily a vertex, can be inside polygon)
        glm::vec3 getClosestPointInMesh(const glm::vec3& position);

        /// Computes path from 'pointA' to 'pointB', first transforming pointA and pointB via a similar method to getClosestPointInMesh first.
        NavPath computePath(const glm::vec3& pointA, const glm::vec3& pointB);

        /// Writes a .cnav file with the contents of this navmesh
        void serialize(Carrot::IO::FileHandle& output) const;

        bool hasTriangles() const;

    private:
        /// Position inside the nav mesh
        struct NavMeshPosition {
            std::size_t triangleIndex = 0;
            glm::vec3 position {0.0f};
        };

        struct NavMeshTriangle {
            std::size_t index = ~0ull;
            Math::Triangle triangle;
            glm::vec3 center{ 0.0f };

            std::size_t globalVertexIndices[3] = { ~0ull }; //< not used at runtime, not serialized
        };

        void funnel(const NavMeshPosition& startPos, const NavMeshPosition& endPos, std::span<const std::size_t> triangles, std::vector<glm::vec3>& waypoints);

        NavMeshPosition getClosestPosition(const glm::vec3& position);

    private:
        // triangle -> other triangle -> shared vertices
        std::unordered_map<std::size_t, std::unordered_map<std::size_t, std::array<glm::vec3, 2>>> portalVertices;

        // nodes are triangles here
        AStar<NavMeshTriangle> pathfinder;

        friend IO::VectorWriter& operator<<(IO::VectorWriter& o, const NavMesh::NavMeshTriangle& triangle);
        friend IO::VectorReader& operator>>(IO::VectorReader& o, NavMesh::NavMeshTriangle& triangle);
        friend IO::VectorWriter& operator<<(IO::VectorWriter& o, const Edge& edge);
        friend IO::VectorReader& operator>>(IO::VectorReader& o, Edge& edge);
    };

    IO::VectorWriter& operator<<(IO::VectorWriter& o, const NavMesh::NavMeshTriangle& triangle);
    IO::VectorReader& operator>>(IO::VectorReader& o, NavMesh::NavMeshTriangle& triangle);
    IO::VectorWriter& operator<<(IO::VectorWriter& o, const Edge& edge);
    IO::VectorReader& operator>>(IO::VectorReader& o, Edge& edge);

} // Carrot::AI
