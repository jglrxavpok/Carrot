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

        NavMeshBuilder() = default;

    public:
        void start(std::vector<MeshEntry>&& entries, float voxelSize, float maxSlope);

    public:
        bool isRunning();

        const std::string& getDebugStep() const {
            return debugStep;
        }

        void debugDraw(const Carrot::Render::Context& renderContext);

    private:
        Carrot::Async::Task<void> build(float voxelSize, float maxSlope);

    private:
        Carrot::Async::Counter taskRunning;
        std::vector<MeshEntry> entries;
        glm::vec3 minVoxelPosition {0.0f};
        float voxelSize = 0.0f;
        glm::vec3 size{0.0f};
        std::size_t sizeX = 0;
        std::size_t sizeY = 0;
        std::size_t sizeZ = 0;

        SparseVoxelGrid voxels;
        std::string debugStep = "Idle";
    };

} // Carrot::AI
