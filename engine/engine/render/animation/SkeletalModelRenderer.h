//
// Created by jglrxavpok on 07/08/2022.
//

#pragma once

#include <memory>
#include <engine/render/resources/PerFrame.h>
#include <engine/render/Model.h>
#include <engine/render/RenderContext.h>
#include <engine/render/ComputePipeline.h>
#include <engine/render/resources/LightMesh.h>

namespace Carrot {
    class BLASHandle;
    class InstanceHandle;
}

namespace Carrot::Render {
    //! Takes a model and handles skinning & rendering for you
    class SkeletalModelRenderer: public SwapchainAware {
    public:
        SkeletalModelRenderer(Carrot::Model::Ref model);

    public:
        Skeleton& getSkeleton();
        const Skeleton& getSkeleton() const;

    public:
        Carrot::InstanceData& getInstanceData();
        const Carrot::InstanceData& getInstanceData() const;

    public:
        /// Skinnes the input model with the skeleton, and renders the resulting meshes
        void onFrame(const Carrot::Render::Context& renderContext);

    public: // SwapchainAware
        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    private:
        void createGPUSkeletonBuffers();
        void createSkinningPipelines();

    private:
        void forEachMesh(const std::function<void(std::uint32_t meshIndex, std::uint32_t materialSlot, Mesh::Ref& mesh)>& action);

    private:
        // TODO: upgrade to texture like Model uses?
        constexpr static std::size_t MAX_BONES_PER_MESH = 40;
        struct GPUSkeleton {
            glm::mat4 boneTransforms[MAX_BONES_PER_MESH]{};
        };

        Carrot::Model::Ref model;
        std::unordered_map<std::string, glm::mat4> transforms;
        std::vector<GPUSkeleton> processedSkeletons; // one per mesh
        std::shared_ptr<Carrot::Pipeline> renderingPipeline = nullptr;
        Carrot::InstanceData instanceData;
        PerFrame<std::vector<std::unique_ptr<Carrot::ComputePipeline>>> skinningPipelines; // [swapchainIndex][mesh]
        PerFrame<std::vector<vk::UniqueSemaphore>> skinningSemaphores; // [swapchainIndex][mesh]
        PerFrame<std::unique_ptr<Carrot::Buffer>> outputBuffers;
        PerFrame<std::vector<std::shared_ptr<Carrot::Mesh>>> renderingMeshes; // one per mesh inside the input model, per swapchain index
        std::vector<glm::mat4> meshTransforms; // one per mesh inside the input model
        PerFrame<std::vector<std::unique_ptr<Carrot::Buffer>>> gpuSkeletons; // one per mesh

        PerFrame<std::shared_ptr<Carrot::BLASHandle>> blas;
        PerFrame<std::shared_ptr<Carrot::InstanceHandle>> rtInstance;
    };
}
