//
// Created by jglrxavpok on 07/08/2022.
//

#include "SkeletalModelRenderer.h"
#include "engine/render/DrawData.h"
#include <engine/render/resources/Mesh.h>
#include <engine/render/resources/ResourceAllocator.h>
#include <engine/render/animation/Animation.h>
#include <engine/render/raytracing/ASBuilder.h>
#include <utility>

namespace Carrot::Render {
    static constexpr std::uint32_t VertexSet = 0;
    static constexpr std::uint32_t InputVerticesBinding = 0;
    static constexpr std::uint32_t OutputVerticesBinding = 1;

    static constexpr std::uint32_t SkeletonSet = 1;
    static constexpr std::uint32_t SkeletonStorageBinding = 0;

    static constexpr std::uint32_t VertexCountConstantID = 0;

    using OutputVertexFormat = Carrot::Vertex;

    SkeletalModelRenderer::SkeletalModelRenderer(Carrot::Model::Ref model): model(std::move(model)) {
        renderingPipeline = GetRenderer().getOrCreatePipeline("gBuffer");
        createGPUSkeletonBuffers();
        createSkinningPipelines();

        instanceData.transform = glm::identity<glm::mat4>();
    }

    void SkeletalModelRenderer::onFrame(const Carrot::Render::Context& renderContext) {
        // compute bone transforms
        getSkeleton().computeTransforms(transforms);
        const auto& boneMapping = model->getBoneMapping();
        for(auto& [boneName, boneTransform] : transforms) {
            auto mappingIt = boneMapping.find(boneName);

            // skeleton can contain scene elements that do not represent a used bone
            if(mappingIt == boneMapping.end()) {
                continue;
            }
            std::uint32_t boneIndex = mappingIt->second;
            verify(boneIndex < Carrot::MAX_BONES_PER_MESH, "Too many bones");
            processedSkeleton.boneTransforms[boneIndex] = boneTransform * model->getBoneOffsetMatrices().at(boneName);
        }

        // upload new skeleton
        gpuSkeletons[renderContext.swapchainIndex]->directUpload(&processedSkeleton, sizeof(GPUSkeleton));

        auto& skinningPipelinesOfThisFrame = skinningPipelines[renderContext.swapchainIndex];
        auto skinnedMeshes = model->getSkinnedMeshes();

        std::size_t meshIndex = 0;
        for(const auto& [mat, meshList] : skinnedMeshes) {
            for(const auto& mesh : meshList) {
                std::uint32_t vertexCount = mesh->getVertexCount();
                std::uint32_t vertexGroups = (vertexCount + 127) / 128;
                auto& semaphore = skinningSemaphores[renderContext.swapchainIndex][meshIndex];

                skinningPipelinesOfThisFrame[meshIndex]->dispatch(vertexGroups, 1, 1, &(*semaphore));

                GetEngine().addWaitSemaphoreBeforeRendering(vk::PipelineStageFlagBits::eVertexInput, *semaphore);

                meshIndex++;
            }
        }

        if(GetCapabilities().supportsRaytracing) {
            blas[renderContext.swapchainIndex]->setDirty();
            rtInstance[renderContext.swapchainIndex]->transform = instanceData.transform;
        }

        // push mesh rendering
        {
            ZoneScopedN("SkeletalModelRenderer Push mesh rendering");
            Carrot::DrawData data;

            Render::Packet& packet = GetRenderer().makeRenderPacket(Carrot::Render::PassEnum::OpaqueGBuffer, renderContext.viewport);
            packet.pipeline = renderingPipeline;
            packet.transparentGBuffer.zOrder = 0.0f;

            Render::Packet::PushConstant& pushConstant = packet.addPushConstant();
            pushConstant.id = "drawDataPush";
            pushConstant.stages = vk::ShaderStageFlagBits::eFragment;

            {
                ZoneScopedN("instance use");
                packet.useInstance(instanceData);
            }

            forEachMesh([&](std::uint32_t meshIndex, std::uint32_t materialSlot, const Mesh::Ref& mesh) {
                ZoneScopedN("mesh use");
                data.materialIndex = materialSlot;
                packet.useMesh(*renderingMeshes[renderContext.swapchainIndex][meshIndex]);

                pushConstant.setData(data);

                renderContext.renderer.render(packet);
            });
        }
    }

    void SkeletalModelRenderer::createGPUSkeletonBuffers() {
        gpuSkeletons.clear();
        gpuSkeletons.reserve(GetEngine().getSwapchainImageCount());

        for (std::size_t i = 0; i < GetEngine().getSwapchainImageCount(); ++i) {
            auto& buffer = gpuSkeletons.emplace_back(GetEngine().getResourceAllocator().allocateDedicatedBuffer(
                    sizeof(GPUSkeleton),
                    vk::BufferUsageFlagBits::eStorageBuffer,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent // we assume the user will update the skeleton very often
                    ));
        }
    }

    void SkeletalModelRenderer::createSkinningPipelines() {
        skinningPipelines.clear();
        skinningPipelines.resize(GetEngine().getSwapchainImageCount());

        skinningSemaphores.clear();
        skinningSemaphores.resize(GetEngine().getSwapchainImageCount());

        outputBuffers.clear();
        outputBuffers.reserve(GetEngine().getSwapchainImageCount());

        renderingMeshes.clear();
        renderingMeshes.resize(GetEngine().getSwapchainImageCount());

        blas.clear();
        blas.reserve(GetEngine().getSwapchainImageCount());

        rtInstance.clear();
        rtInstance.reserve(GetEngine().getSwapchainImageCount());

        auto skinnedMeshes = model->getSkinnedMeshes();

        // compute required size for output buffer
        std::size_t requiredSize = 0;

        // keep track of where the output mesh will be inside the output buffer
        std::vector<std::size_t> offsets;
        std::vector<std::size_t> sizes;
        offsets.reserve(skinnedMeshes.size());
        sizes.reserve(skinnedMeshes.size());
        forEachMesh([&](std::uint32_t meshIndex, std::uint32_t materialSlot, const Mesh::Ref& mesh) {
            offsets.emplace_back(requiredSize);

            std::size_t outputMeshSize = mesh->getVertexCount() * sizeof(OutputVertexFormat);
            sizes.emplace_back(outputMeshSize);
            requiredSize += outputMeshSize;
        });


        for (std::size_t imageIndex = 0; imageIndex < GetEngine().getSwapchainImageCount(); imageIndex++) {
            // create output buffer for this frame
            auto& outputBuffer = outputBuffers.emplace_back(GetEngine().getResourceAllocator().allocateDedicatedBuffer(requiredSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal));

            // create skinning pipelines for all meshes inside the model
            auto& pipelinesForThisFrame = skinningPipelines[imageIndex];
            auto& semaphoresForThisFrame = skinningSemaphores[imageIndex];
            forEachMesh([&](std::uint32_t meshIndex, std::uint32_t materialSlot, const Mesh::Ref& mesh) {
                Carrot::ComputePipelineBuilder builder(GetEngine());
                builder.shader("resources/shaders/compute/skeleton-skinning.compute.glsl.spv");

                // input vertices
                builder.bufferBinding(vk::DescriptorType::eStorageBuffer, VertexSet, InputVerticesBinding,
                                      mesh->getVertexBuffer().asBufferInfo());

                // output vertices
                Carrot::BufferView outputBufferView {
                    nullptr,
                    *outputBuffer,
                    offsets[meshIndex],
                    sizes[meshIndex],
                };
                builder.bufferBinding(vk::DescriptorType::eStorageBuffer, VertexSet, OutputVerticesBinding,
                                      outputBufferView.asBufferInfo());

                // skeleton buffer
                builder.bufferBinding(vk::DescriptorType::eStorageBuffer, SkeletonSet, SkeletonStorageBinding,
                                      gpuSkeletons[imageIndex]->asBufferInfo());

                // specialization
                builder.specializationUInt32(VertexCountConstantID, mesh->getVertexCount());

                pipelinesForThisFrame.emplace_back(builder.build());
                semaphoresForThisFrame.emplace_back(GetVulkanDevice().createSemaphoreUnique({}));

                // create corresponding rendering meshes
                renderingMeshes[imageIndex].emplace_back(std::make_shared<Carrot::LightMesh>(outputBufferView, mesh->getIndexBuffer(), sizeof(OutputVertexFormat)));
            });

            if(GetCapabilities().supportsRaytracing) {
                auto& asBuilder = GetRenderer().getASBuilder();
                auto& createdBLAS = blas.emplace_back(asBuilder.addBottomLevel(renderingMeshes[imageIndex]));
                rtInstance.emplace_back(asBuilder.addInstance(createdBLAS));
            }
        }
    }

    void SkeletalModelRenderer::onSwapchainImageCountChange(size_t newCount) {
        createGPUSkeletonBuffers();
        createSkinningPipelines();
    }

    void SkeletalModelRenderer::onSwapchainSizeChange(int newWidth, int newHeight) {
        // no-op
    }

    void SkeletalModelRenderer::forEachMesh(const std::function<void(std::uint32_t meshIndex, std::uint32_t materialSlot, Mesh::Ref& mesh)>& action) {
        std::size_t meshIndex = 0;
        for(auto& [materialSlot, meshList] : model->getSkinnedMeshes()) {
            for(auto& mesh : meshList) {
                action(meshIndex, materialSlot, mesh);
                meshIndex++;
            }
        }
    }

    Skeleton& SkeletalModelRenderer::getSkeleton() {
        return model->getSkeleton();
    }

    const Skeleton& SkeletalModelRenderer::getSkeleton() const {
        return model->getSkeleton();
    }

    InstanceData& SkeletalModelRenderer::getInstanceData() {
        return instanceData;
    }
}
