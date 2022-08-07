//
// Created by jglrxavpok on 07/08/2022.
//

#pragma once

#include <memory>
#include <engine/render/resources/PerFrame.h>
#include <engine/render/Model.h>
#include <engine/render/RenderContext.h>
#include <engine/render/ComputePipeline.h>

namespace Carrot::Render {
    //! Takes a model and handles skinning & rendering for you
    class SkeletalModelRenderer: public SwapchainAware {
    public:
        SkeletalModelRenderer(Carrot::Model::Ref model);

        Skeleton& getSkeleton();
        const Skeleton& getSkeleton() const;

        void onFrame(const Carrot::Render::Context& renderContext);

    public: // SwapchainAware
        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    private:
        void createGPUSkeletonBuffers();
        void createSkinningPipelines();

    private:
        Carrot::Model::Ref model;
        PerFrame<std::vector<std::unique_ptr<Carrot::ComputePipeline>>> skinningPipelines; // [swapchainIndex][mesh]
        PerFrame<std::unique_ptr<Carrot::Buffer>> gpuSkeletons;
    };
}
