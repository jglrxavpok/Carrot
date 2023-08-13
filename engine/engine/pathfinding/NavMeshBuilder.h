//
// Created by jglrxavpok on 02/08/2023.
//

#pragma once

#include <engine/pathfinding/SparseVoxelGrid.h>
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
        bool isRunning();

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

        struct Region {
            std::size_t index = 0; //< index of this region
            glm::ivec3 center { 0 };
            std::unordered_set<std::size_t> connected; //< indices of regions connected to this one (does not contain itself)

            std::vector<glm::ivec3> contour;
        };


        using OpenHeightField = SparseArray<HeightFieldColumn>;

        Carrot::Async::Task<void> build();

        bool doSpansConnect(const HeightFieldSpan& spanA, const HeightFieldSpan& spanB);

        void buildOpenHeightField(const SparseVoxelGrid& voxels, OpenHeightField& field);
        void buildDistanceField(OpenHeightField& field);

        /**
         * Flood-fills the field with the given region (base), filling connected spans that have the same distance to the border
         * @param field
         * @param base
         */
        void floodFill(OpenHeightField& field, const Region& base);
        void buildRegions(OpenHeightField& field, std::vector<Region>& regions);

        void buildContours(const OpenHeightField& field, std::vector<Region>& regions);
        void buildContour(const OpenHeightField& field, Region& region);

    private:
        Carrot::Async::Counter taskRunning;
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
        };
        WorkingData workingData;

        std::string debugStep = "Idle";
    };

} // Carrot::AI
