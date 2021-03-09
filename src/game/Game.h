//
// Created by jglrxavpok on 05/12/2020.
//

#pragma once

#include <cstdint>
#include "engine/vulkan/includes.h"
#include "engine/Engine.h"
#include <engine/ecs/World.h>
#include "engine/render/InstanceData.h"

namespace Carrot {
    class Engine;
}

namespace Game {
    using namespace Carrot;

    class Game {
    private:
        Engine& engine;
        unique_ptr<Model> mapModel = nullptr;
        unique_ptr<Model> model = nullptr;
        unique_ptr<Buffer> instanceBuffer = nullptr;
        unique_ptr<Buffer> mapInstanceBuffer = nullptr;
        unique_ptr<Buffer> fullySkinnedUnitVertices = nullptr;
        unique_ptr<Buffer> flatVertices = nullptr;
        map<MeshID, shared_ptr<Buffer>> indirectBuffers{};
        AnimatedInstanceData* modelInstance = nullptr;

        vector<vk::UniqueDescriptorPool> computeDescriptorPools{};
        vector<vk::DescriptorSet> computeDescriptorSet0{};
        vector<vk::DescriptorSet> computeDescriptorSet1{};
        vk::UniqueDescriptorSetLayout computeSetLayout0{};
        vk::UniqueDescriptorSetLayout computeSetLayout1{};
        vk::UniquePipelineLayout computePipelineLayout{};
        vk::UniquePipeline computePipeline{};
        vector<vk::CommandBuffer> skinningCommandBuffers{};
        vector<vk::UniqueSemaphore> skinningSemaphores{};

        World world;

        void createSkinningComputePipeline(uint64_t vertexCountPerInstance);

    public:
        explicit Game(Engine& engine);

        void onFrame(uint32_t frameIndex);

        void recordGBufferPass(uint32_t frameIndex, vk::CommandBuffer& commands);

        void onMouseMove(double dx, double dy);

        void changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages);
    };
}
