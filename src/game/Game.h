//
// Created by jglrxavpok on 05/12/2020.
//

#pragma once

#include <cstdint>
#include "vulkan/includes.h"
#include "Engine.h"
#include "Unit.h"

namespace Carrot {
    class Engine;

    class Game {
    private:
        Engine& engine;
        unique_ptr<Model> mapModel = nullptr;
        unique_ptr<Model> model = nullptr;
        unique_ptr<Buffer> instanceBuffer = nullptr;
        unique_ptr<Buffer> mapInstanceBuffer = nullptr;
        map<MeshID, shared_ptr<Buffer>> indirectBuffers{};
        AnimatedInstanceData* modelInstance = nullptr;
        vector<unique_ptr<Unit>> units{};
        vk::UniquePipelineLayout computePipelineLayout{};
        vk::UniquePipeline computePipeline{};
        vector<vk::CommandBuffer> skinningCommandBuffers{};
        vector<vk::UniqueSemaphore> skinningSemaphores{};

        void createSkinningComputePipeline(const vector<uint32_t>& meshSizes, uint64_t maxVertexCount);

    public:
        explicit Game(Engine& engine);

        void onFrame(uint32_t frameIndex);

        void recordCommandBuffer(uint32_t frameIndex, vk::CommandBuffer& commands);

        void onMouseMove(double dx, double dy);

        void changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages);
    };
}
