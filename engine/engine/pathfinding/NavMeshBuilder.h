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
        struct BuildParams {
            float voxelSize = 0.5f;
            float maxSlope = glm::radians(45.0f);
            float characterHeight = 1.8f;
            float characterRadius = 0.5f;
        };
        void start(std::vector<MeshEntry>&& entries, const BuildParams& params);

    public:
        bool isRunning();

        const std::string& getDebugStep() const {
            return debugStep;
        }

        void debugDraw(const Carrot::Render::Context& renderContext);

    private:
        Carrot::Async::Task<void> build();

    private:
        Carrot::Async::Counter taskRunning;
        std::vector<MeshEntry> entries;
        glm::vec3 minVoxelPosition {0.0f};
        BuildParams params;
        glm::vec3 size{0.0f};
        std::size_t sizeX = 0;
        std::size_t sizeY = 0;
        std::size_t sizeZ = 0;

        SparseVoxelGrid voxels;
        std::vector<glm::ivec3> debugVoxelPositions;
        std::string debugStep = "Idle";
    };

} // Carrot::AI
