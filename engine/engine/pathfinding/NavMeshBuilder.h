//
// Created by jglrxavpok on 02/08/2023.
//

#pragma once

#include <engine/pathfinding/SparseVoxelGrid.h>
#include <engine/pathfinding/NavMesh.h>
#include <engine/render/Model.h>
#include <core/async/Counter.h>
#include <core/async/Coroutines.hpp>
#include <glm/gtx/hash.hpp>

namespace Carrot::AI {

    class NavMeshBuilder {
    public:
        struct MeshEntry {
            std::shared_ptr<Carrot::Model> model;
            glm::mat4 transform { 1.0f };
        };

        enum class DebugDrawType {
            WalkableVoxels,
            OpenHeightField,
            DistanceField,
            Regions,
            Contours,
            SimplifiedContours,
            RegionMeshes,
            Mesh,
        };

        NavMeshBuilder() = default;

    public:
        struct BuildParams {
            float voxelSize = 0.5f;
            float maxSlope = glm::radians(45.0f);
            std::size_t characterHeight = 1; //< size of character, in voxels
            std::size_t characterRadius = 1; //< size of character, in voxels

            std::size_t maxClimbHeight = 1; //< Max step size, in voxels
        };
        void start(std::vector<MeshEntry>&& entries, const BuildParams& params);

    public:
        bool isRunning() const;

        const NavMesh& getResult() const;

        const std::string& getDebugStep() const {
            return debugStep;
        }

        void debugDraw(const Carrot::Render::Context& renderContext, DebugDrawType drawType);

    private:
        constexpr static std::uint8_t Right = 0;
        constexpr static std::uint8_t Forward = 1;
        constexpr static std::uint8_t Left = 2;
        constexpr static std::uint8_t Backwards = 3;
        constexpr static std::uint8_t DirectionCount = 4;
        constexpr static std::int8_t Dx[DirectionCount] {
                1,0,-1,0
        };
        constexpr static std::int8_t Dy[DirectionCount] {
                0,1,0,-1
        };
        constexpr static std::int8_t PreviousDirection[DirectionCount] {
                Backwards, Right, Forward, Left
        };
        constexpr static std::int8_t NextDirection[DirectionCount] {
                Forward, Left, Backwards, Right
        };

        struct HeightFieldSpan {
            std::int64_t bottomZ = 0; //< bottom z-level of this span
            std::int64_t height = 0; //< height, in voxels, of this span

            std::int64_t distanceToBorder = 0;
            std::size_t regionID = 0; //< if > 0, index+1 of the region this span is inside, if 0, no region assigned yet

            bool connected[DirectionCount] { false }; //< has connection is the given direction?
        };

        struct HeightFieldColumn {
            std::vector<HeightFieldSpan> spans;
        };

        struct ContourPoint {
            std::int64_t x = 0;
            std::int64_t y = 0;
            std::size_t spanIndex = 0;

            std::uint8_t edgeDirection = 0; //< Direction in which this point was created. Helps determine where to place vertices on the actual border (not at the center of voxels)

            bool operator==(const ContourPoint&) const;
        };

        struct Graph {
            struct Face {
                std::size_t indexA = 0;
                std::size_t indexB = 0;
                std::size_t indexC = 0;
            };

            std::vector<glm::vec3> points;
            std::vector<Face> faces;
        };

        struct Region {
            std::size_t index = 0; //< index of this region
            glm::ivec3 center { 0 };
            std::vector<glm::ivec3> inside;

            std::vector<ContourPoint> contour;
            std::vector<ContourPoint> simplifiedContour;

            Graph triangulatedRegion;
            std::unique_ptr<Carrot::Mesh> triangulatedRegionMesh;
        };

        using OpenHeightField = SparseArray<HeightFieldColumn>;

        Carrot::Async::Task<void> build();

        bool doSpansConnect(const HeightFieldSpan& spanA, const HeightFieldSpan& spanB);
        bool doContourPointsConnect(const OpenHeightField& field, const ContourPoint& pointA, const ContourPoint& pointB);

        /// Converts a contour point to world space
        glm::vec3 contourToWorld(const OpenHeightField& field, const ContourPoint& point);

        /// Like contourToWorld, but lerps Z with neighbor if there is one (to handle contour points with different height)
        glm::vec3 contourToWorldBorderAware(const OpenHeightField& field, const ContourPoint& point, const Region& originalRegion, bool& isShared);

        void buildOpenHeightField(const SparseVoxelGrid& voxels, OpenHeightField& field);
        void buildDistanceField(OpenHeightField& field);
        void narrowDistanceField(OpenHeightField& field);

        /**
         * Flood-fills the field with the given region (base), filling connected spans that have the same distance to the border
         * @param field
         * @param base
         */
        void floodFill(OpenHeightField& field, const Region& base);
        void buildRegions(OpenHeightField& field, std::vector<Region>& regions);

        void buildContours(const OpenHeightField& field, std::vector<Region>& regions);
        void buildContour(const OpenHeightField& field, Region& region);

        void simplifyContours(const OpenHeightField& field, std::vector<Region>& regions);
        void simplifyContour(const OpenHeightField& field, Region& region);

        std::unique_ptr<Carrot::Mesh> graphToMesh(const Graph& graph);
        void triangulateContour(const OpenHeightField& field, const std::vector<Region>& regions, Region& region);
        void buildMesh(const OpenHeightField& field, std::vector<Region>& region, Graph& rawMesh);
        void makeNavMesh(const Graph& rawMesh, NavMesh& navMesh);

    private:
        Carrot::Async::Counter taskRunning;
        NavMesh navMesh;

        std::vector<MeshEntry> entries;
        glm::vec3 minVoxelPosition {0.0f};
        BuildParams params;
        glm::vec3 size{0.0f};
        std::size_t sizeX = 0;
        std::size_t sizeY = 0;
        std::size_t sizeZ = 0;

        struct WorkingData {
            SparseVoxelGrid voxelizedScene;
            SparseVoxelGrid walkableVoxels;
            OpenHeightField openHeightField;
            std::int64_t maxDistance = 1;
            std::vector<Region> regions;

            Graph rawMesh;
            std::unique_ptr<Carrot::Mesh> debugRawMesh;
        };
        WorkingData workingData;

        std::string debugStep = "Idle";
    };

} // Carrot::AI
