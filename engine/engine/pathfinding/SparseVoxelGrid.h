//
// Created by jglrxavpok on 05/08/2023.
//

#pragma once

#include <unordered_set>
#include <glm/gtx/hash.hpp>
#include <core/SparseArray.hpp>

namespace Carrot::AI {

    struct Voxel {
        bool empty = true;
        bool walkable = true;
        //glm::vec3 averageNormal {0.0f};
    };

    class SparseVoxelGrid {
    public:
        struct Iterator {
        public:
            struct PositionVoxelPair {
                glm::ivec3 position;
                Voxel& voxel;
            };

            PositionVoxelPair operator->() const;
            PositionVoxelPair operator*() const;

            bool operator==(const Iterator& other) const;
            Iterator& operator++();

        private:
            SparseVoxelGrid* grid = nullptr;
            std::size_t index = ~0ull;

            friend class SparseVoxelGrid;
        };

        SparseVoxelGrid() = default;

        void reset(std::size_t sizeX, std::size_t sizeY, std::size_t sizeZ);

        /**
         * Reference is valid only until the next call to 'insert'
         */
        Voxel& insert(std::size_t x, std::size_t y, std::size_t z);

        /**
         * Reference is valid only until the next call to 'insert'
         */
        Voxel& insert(const glm::uvec3& xyz);

        void finishBuild();

        Iterator begin();
        Iterator end();

    private:
        std::size_t posToIndex(const glm::ivec3& pos);

        std::size_t sizeX = 0;
        std::size_t sizeY = 0;
        std::size_t sizeZ = 0;

        SparseArray<Voxel, 1024> voxelizedMeshes;
        std::unordered_set<glm::ivec3> nonEmptyVoxelsSet;
        std::vector<glm::ivec3> nonEmptyVoxels;

        friend class Iterator;
    };

} // Carrot::AI
