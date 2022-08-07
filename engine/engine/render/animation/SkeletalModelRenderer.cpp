//
// Created by jglrxavpok on 07/08/2022.
//

#include "SkeletalModelRenderer.h"
#include <engine/render/resources/Mesh.h>
#include <utility>

namespace Carrot::Render {
    SkeletalModelRenderer::SkeletalModelRenderer(Carrot::Model::Ref model): model(std::move(model)) {
        createGPUSkeletonBuffers();
        createSkinningPipelines();
    }

    void SkeletalModelRenderer::onFrame(const Carrot::Render::Context& renderContext) {
        auto& skinningPipelinesOfThisFrame = skinningPipelines[renderContext.swapchainIndex];
        auto skinnedMeshes = model->getSkinnedMeshes();
        for(std::size_t meshIndex = 0; meshIndex < skinnedMeshes.size(); meshIndex++) {
            std::uint32_t vertexCount = skinnedMeshes[meshIndex]->getVertexCount();

            // TODO: semaphore & add to engine
            skinningPipelinesOfThisFrame[meshIndex]->dispatch(vertexCount, 1, 1);
        }
    }

    void SkeletalModelRenderer::createGPUSkeletonBuffers() {
        skinningPipelines.clear();
        skinningPipelines.reserve(GetEngine().getSwapchainImageCount());

        gpuSkeletons.clear();
        gpuSkeletons.reserve(GetEngine().getSwapchainImageCount());

        TODO;
    }

    void SkeletalModelRenderer::createSkinningPipelines() {
        TODO;
    }

    void SkeletalModelRenderer::onSwapchainImageCountChange(size_t newCount) {
        createGPUSkeletonBuffers();
        createSkinningPipelines();
    }

    void SkeletalModelRenderer::onSwapchainSizeChange(int newWidth, int newHeight) {
        // no-op
    }
}
