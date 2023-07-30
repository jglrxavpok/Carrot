//
// Created by jglrxavpok on 29/07/2023.
//

#pragma once

#include <engine/pathfinding/AStar.h>
#include <engine/pathfinding/NavPath.h>
#include <core/math/Triangle.h>
#include <core/scene/LoadedScene.h>
#include <core/io/Resource.h>

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

    private:
        /// Position inside the nav mesh
        struct NavMeshPosition {
            std::size_t triangleIndex = 0;
            glm::vec3 position {0.0f};
        };

        struct NavMeshTriangle {
            std::size_t index = ~0ull;
            Math::Triangle triangle;
            std::size_t globalVertexIndices[3] = { ~0ull };
            glm::vec3 center{ 0.0f };
        };

        NavMeshPosition getClosestPosition(const glm::vec3& position);

    private:
        // nodes are triangles here
        AStar<NavMeshTriangle> pathfinder;
    };

} // Carrot::AI
