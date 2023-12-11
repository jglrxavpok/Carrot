//
// Created by jglrxavpok on 12/11/2023.
//

#include "VisibilityBuffer.h"
#include <engine/render/VulkanRenderer.h>
#include <engine/render/TextureRepository.h>
#include <engine/utils/Profiling.h>
#include <engine/Engine.h>

#include "ClusterManager.h"

namespace Carrot::Render {

    VisibilityBuffer::VisibilityBuffer(VulkanRenderer& renderer): renderer(renderer) {

    }

    struct VisibilityBufferRasterizationData {
        Render::FrameResource visibilityBuffer;
        Render::FrameResource depthStencil;
    };

    const VisibilityBuffer::VisibilityPassData& VisibilityBuffer::addVisibilityBufferPasses(Render::GraphBuilder& graph, const Render::PassData::GBuffer& gBufferData, const Render::TextureSize& framebufferSize) {
        // Add the hardware rasterization pass
        auto& rasterizePass = graph.addPass<VisibilityBufferRasterizationData>("rasterize visibility buffer",
            [this, &gBufferData, framebufferSize](Render::GraphBuilder& builder, Render::Pass<VisibilityBufferRasterizationData>& pass, VisibilityBufferRasterizationData& data) {
                // Declare the visibility buffer texture
                data.visibilityBuffer = builder.createStorageTarget("visibility buffer", vk::Format::eR64Uint, framebufferSize, vk::ImageLayout::eGeneral);
                data.depthStencil = builder.write(gBufferData.depthStencil, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
            },
            [this](const Render::CompiledPass& pass, const Render::Context& frame, const VisibilityBufferRasterizationData& data, vk::CommandBuffer& cmds) {
                ZoneScopedN("CPU RenderGraph visibility buffer rasterize");
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "visibility buffer rasterize");
                auto& texture = pass.getGraph().getTexture(data.visibilityBuffer, frame.swapchainIndex);

                auto clearPipeline = renderer.getOrCreatePipelineFullPath("resources/pipelines/compute/clear-visibility-buffer.json");
                renderer.bindStorageImage(*clearPipeline, frame, texture, 0, 0,
                                          vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                const auto& extent = texture.getSize();
                const std::uint8_t localSize = 32;
                std::size_t dispatchX = (extent.width + (localSize-1)) / localSize;
                std::size_t dispatchY = (extent.height + (localSize-1)) / localSize;

                clearPipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);
                cmds.dispatch(dispatchX, dispatchY, 1);

                vk::MemoryBarrier2KHR memoryBarrier {
                        .srcStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                        .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                        .dstStageMask = vk::PipelineStageFlagBits2KHR::eFragmentShader,
                        .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead,
                };
                vk::DependencyInfoKHR dependencyInfo {
                        .memoryBarrierCount = 1,
                        .pMemoryBarriers = &memoryBarrier,
                };
                cmds.pipelineBarrier2KHR(dependencyInfo);

                // Record all render packets (~= draw commands) with the tag "VisibilityBuffer"

                // instanceIndex must match with one used in MeshletManager to reference the proper pipeline
                auto pipeline = renderer.getOrCreatePipelineFullPath("resources/pipelines/visibility-buffer.json", (std::uint64_t)frame.pViewport);
                renderer.bindStorageImage(*pipeline, frame, texture, 0, 3,
                                          vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                frame.renderer.recordPassPackets(Render::PassEnum::VisibilityBuffer, pass.getRenderPass(), frame, cmds);
            }
        );

        auto& debugPass = graph.addPass<PassData::PostProcessing>("visibility buffer debug",
           [this, rasterizePass, framebufferSize](Render::GraphBuilder& builder, Render::Pass<PassData::PostProcessing>& pass, PassData::PostProcessing& data) {
               data.postLighting = builder.read(rasterizePass.getData().visibilityBuffer, vk::ImageLayout::eGeneral);
               data.postProcessed = builder.createRenderTarget("Debug",
                                                               vk::Format::eR8G8B8A8Unorm,
                                                               framebufferSize,
                                                               vk::AttachmentLoadOp::eClear,
                                                               vk::ClearColorValue(std::array{0,0,0,1}),
                                                               vk::ImageLayout::eGeneral
               );
               GetEngine().getVulkanDriver().getTextureRepository().getUsages(data.postProcessed.rootID) |= vk::ImageUsageFlagBits::eSampled;
           },
           [this](const Render::CompiledPass& pass, const Render::Context& frame, const PassData::PostProcessing& data, vk::CommandBuffer& cmds) {
               ZoneScopedN("CPU RenderGraph visibility buffer debug view");
               TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "visibility buffer debug view");
               auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("post-process/visibility-buffer", pass.getRenderPass());
               auto& texture = pass.getGraph().getTexture(data.postLighting, frame.swapchainIndex);
               renderer.bindStorageImage(*pipeline, frame, texture, 0, 0,
                                    vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

               pipeline->bind(pass.getRenderPass(), frame, cmds);
               auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
               screenQuadMesh.bind(cmds);
               screenQuadMesh.draw(cmds);
               //texture.transitionInline(cmds, vk::ImageLayout::eShaderReadOnlyOptimal);
           }
        );

        // Take visibility buffer as input and render materials to GBuffer
        auto& materialPass = graph.addPass<VisibilityPassData>("visibility buffer materials",
            [this, &gBufferData, &rasterizePass, &debugPass](Render::GraphBuilder& builder, Render::Pass<VisibilityPassData>& pass, VisibilityPassData& data) {
                data.gbuffer.writeTo(builder, gBufferData);
                data.visibilityBuffer = builder.read(rasterizePass.getData().visibilityBuffer, vk::ImageLayout::eGeneral);
                data.debugView = debugPass.getData().postProcessed;
            },
            [this](const Render::CompiledPass& pass, const Render::Context& frame, const VisibilityPassData& data, vk::CommandBuffer& cmds) {
                Carrot::BufferView clusters = frame.renderer.getMeshletManager().getClusters(frame);
                Carrot::BufferView clusterInstances = frame.renderer.getMeshletManager().getClusterInstances(frame);
                Carrot::BufferView clusterInstanceData = frame.renderer.getMeshletManager().getClusterInstanceData(frame);
                if(!clusters || !clusterInstances || !clusterInstanceData) {
                    return;
                }

                auto pipeline = frame.renderer.getOrCreatePipelineFullPath("resources/pipelines/material-pass.json");
                const auto& visibilityBufferTexture = pass.getGraph().getTexture(data.visibilityBuffer, frame.swapchainIndex);
                data.gbuffer.bindInputs(*pipeline, frame, pass.getGraph(), 0, vk::ImageLayout::eGeneral);
                frame.renderer.getMaterialSystem().bind(frame, cmds, 1, pipeline->getPipelineLayout());
                frame.renderer.bindStorageImage(*pipeline, frame, visibilityBufferTexture, 2, 0,
                    vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                frame.renderer.bindBuffer(*pipeline, frame, clusters, 2, 1);
                frame.renderer.bindBuffer(*pipeline, frame, clusterInstances, 2, 2);
                frame.renderer.bindBuffer(*pipeline, frame, clusterInstanceData, 2, 3);

                pipeline->bind(pass.getRenderPass(), frame, cmds);
                auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                screenQuadMesh.bind(cmds);
                screenQuadMesh.draw(cmds);
            }
        );

        return materialPass.getData();
    }
} // Carrot::Render