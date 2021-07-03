//
// Created by jglrxavpok on 16/04/2021.
//

#pragma once

#include "engine/Engine.h"
#include "engine/render/InstanceData.h"

namespace Carrot {
    class AnimatedInstances {
    private:
        size_t maxInstanceCount = 0;
        Carrot::Engine& engine;
        shared_ptr<Model> model = nullptr;
        unique_ptr<Buffer> fullySkinnedUnitVertices = nullptr;
        unique_ptr<Buffer> flatVertices = nullptr;
        map<MeshID, shared_ptr<Buffer>> indirectBuffers{};
        AnimatedInstanceData* animatedInstances = nullptr;
        unique_ptr<Buffer> instanceBuffer = nullptr;

        unordered_map<MeshID, size_t> meshOffsets{};
        size_t vertexCountPerInstance = 0;

        vector<vk::UniqueDescriptorPool> computeDescriptorPools{};
        vector<vk::DescriptorSet> computeDescriptorSet0{};
        vector<vk::DescriptorSet> computeDescriptorSet1{};
        vk::UniqueDescriptorSetLayout computeSetLayout0{};
        vk::UniqueDescriptorSetLayout computeSetLayout1{};
        vk::UniquePipelineLayout computePipelineLayout{};
        vk::UniquePipeline computePipeline{};
        vector<vk::CommandBuffer> skinningCommandBuffers{};
        vector<vk::UniqueSemaphore> skinningSemaphores{};

        void createSkinningComputePipeline();

    public:
        explicit AnimatedInstances(Carrot::Engine& engine, shared_ptr<Model> animatedModel, size_t maxInstanceCount);

    /// Getters
        Model& getModel() { return *model; };

        AnimatedInstanceData* getInstancePtr() { return animatedInstances; };

        AnimatedInstanceData& getInstance(size_t index) {
            assert(index < maxInstanceCount);
            return animatedInstances[index];
        };

        inline vk::DeviceSize getVertexOffset(size_t instanceIndex, MeshID meshID) {
            return static_cast<int32_t>(instanceIndex * vertexCountPerInstance + meshOffsets[meshID]);
        }

        /**
         * Buffer containing all vertices for all instances after skinning
         */
        Buffer& getFullySkinnedBuffer() { return *fullySkinnedUnitVertices; };

        vk::Semaphore& getSkinningSemaphore(size_t frameIndex) { return *skinningSemaphores[frameIndex]; };

#pragma region RenderingUpdate
        void onFrame(size_t frameIndex);

        void recordGBufferPass(vk::RenderPass pass, size_t frameIndex, vk::CommandBuffer& commands, size_t instanceCount);
#pragma endregion RenderingUpdate
    };
}
