//
// Created by jglrxavpok on 05/08/2023.
//

#include "SparseVoxelGrid.h"

namespace Carrot::AI {

    SparseVoxelGrid::Iterator::PositionVoxelPair SparseVoxelGrid::Iterator::operator->() const {
        auto pos = grid->nonEmptyVoxels[index];
        return {
            .position = pos,
            .voxel = grid->voxelizedMeshes[grid->posToIndex(pos)],
        };
    }

    SparseVoxelGrid::Iterator::PositionVoxelPair SparseVoxelGrid::Iterator::operator*() const {
        auto pos = grid->nonEmptyVoxels[index];
        return {
                .position = pos,
                .voxel = grid->voxelizedMeshes[grid->posToIndex(pos)],
        };
    }

    bool SparseVoxelGrid::Iterator::operator==(const SparseVoxelGrid::Iterator& other) const {
        return grid == other.grid && index == other.index;
    }

    SparseVoxelGrid::Iterator& SparseVoxelGrid::Iterator::operator++() {
        index++;
        return *this;
    }

    void SparseVoxelGrid::reset(std::size_t _sizeX, std::size_t _sizeY, std::size_t _sizeZ) {
        sizeX = _sizeX;
        sizeY = _sizeY;
        sizeZ = _sizeZ;
        voxelizedMeshes.clear();
        voxelizedMeshes.resize(sizeX * sizeY * sizeZ);
    }

    Voxel& SparseVoxelGrid::insert(std::size_t x, std::size_t y, std::size_t z) {
        auto& voxel = voxelizedMeshes[x + y * sizeX + z * sizeX * sizeY];
        voxel.empty = false;
        nonEmptyVoxelsSet.insert(glm::ivec3(x,y,z));
        return voxel;
    }

    Voxel& SparseVoxelGrid::insert(const glm::uvec3& xyz) {
        return insert(xyz.x, xyz.y, xyz.z);
    }

    void SparseVoxelGrid::finishBuild() {
        for(auto& v : nonEmptyVoxelsSet) {
            nonEmptyVoxels.emplace_back(std::move(v));
        }
        nonEmptyVoxelsSet.clear();
    }

    SparseVoxelGrid::Iterator SparseVoxelGrid::begin() {
        SparseVoxelGrid::Iterator it;
        it.grid = this;
        it.index = 0;
        return it;
    }

    SparseVoxelGrid::Iterator SparseVoxelGrid::end() {
        SparseVoxelGrid::Iterator it;
        it.grid = this;
        it.index = nonEmptyVoxels.size();
        return it;
    }

    std::size_t SparseVoxelGrid::posToIndex(const glm::ivec3& pos) {
        return pos.x + pos.y * sizeX + pos.z * sizeX * sizeY;
    }

} // Carrot::AI