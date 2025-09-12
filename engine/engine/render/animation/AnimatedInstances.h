//
// Created by jglrxavpok on 16/04/2021.
//

#pragma once

#include "engine/render/IDTypes.h"
#include "engine/render/Model.h"
#include "engine/render/InstanceData.h"

namespace Carrot {
    class Engine;
    class BLASHandle;
    class InstanceHandle;

    //! Used to render one or multiple skinned meshes, while playing their animation.
    //! For programmatic control over the skeleton, use Carrot::Render::Skeleton
    class AnimatedInstances {
    public:
        explicit AnimatedInstances(Carrot::Engine& engine, std::shared_ptr<Model> animatedModel, std::size_t maxInstanceCount);

    /// Getters
        Model& getModel() { return *model; };

        AnimatedInstanceData* getInstancePtr() { return animatedInstances; };

        AnimatedInstanceData& getInstance(std::size_t index) {
            assert(index < maxInstanceCount);
            return animatedInstances[index];
        }

        inline vk::DeviceSize getVertexOffset(std::size_t instanceIndex, MeshID meshID) {
            return static_cast<std::int32_t>(instanceIndex * vertexCountPerInstance + meshOffsets[meshID]);
        }

        void render(const Carrot::Render::Context& renderContext, Carrot::Render::PassName renderPass);
        void render(const Carrot::Render::Context& renderContext, Carrot::Render::PassName renderPass, std::size_t instanceCount);

#pragma region RenderingUpdate
        vk::Semaphore& onFrame(const Carrot::Render::Context& renderContext);
#pragma endregion RenderingUpdate

    private:
        void forEachMesh(const std::function<void(std::uint32_t meshIndex, std::uint32_t materialSlot, std::shared_ptr<Mesh>& mesh)>& action);

    private:
        std::size_t maxInstanceCount = 0;
        std::size_t currentInstanceCount = 0;
        Carrot::Engine& engine;
        std::shared_ptr<Model> model = nullptr;
        BufferAllocation fullySkinnedVertexBuffer;
        std::unique_ptr<Buffer> flatVertices = nullptr;
        std::map<MeshID, std::shared_ptr<Buffer>> indirectBuffers{};
        AnimatedInstanceData* animatedInstances = nullptr;
        std::unique_ptr<Buffer> instanceBuffer = nullptr;
        std::vector<std::shared_ptr<BLASHandle>> raytracingBLASes; // size is instanceCount
        std::vector<std::shared_ptr<InstanceHandle>> raytracingInstances; // size is instanceCount

        std::unordered_map<MeshID, size_t> meshOffsets{};
        std::size_t vertexCountPerInstance = 0;

        vk::UniqueDescriptorPool computeDescriptorPool{};
        vk::DescriptorSet computeDescriptorSet0{};
        vk::DescriptorSet computeDescriptorSet1{};
        vk::UniqueDescriptorSetLayout computeSetLayout0{};
        vk::UniqueDescriptorSetLayout computeSetLayout1{};
        vk::UniquePipelineLayout computePipelineLayout{};
        vk::UniquePipeline computePipeline{};
        vk::CommandBuffer skinningCommandBuffer{};
        vk::UniqueSemaphore skinningSemaphore{};
        bool submitAtLeastOneSkinningCompute = false;

        void createSkinningComputePipeline();
    };
}
