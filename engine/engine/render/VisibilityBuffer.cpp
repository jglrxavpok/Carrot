//
// Created by jglrxavpok on 12/11/2023.
//

#include "VisibilityBuffer.h"
#include <engine/render/VulkanRenderer.h>
#include <engine/render/TextureRepository.h>
#include <engine/utils/Profiling.h>
#include <engine/Engine.h>

#include "ClusterManager.h"
#include "DebugBufferObject.h"

namespace Carrot::Render {

    VisibilityBuffer::VisibilityBuffer(VulkanRenderer& renderer): renderer(renderer) {

    }

    struct VisibilityBufferRasterizationData {
        Render::FrameResource visibilityBuffer;
    };

    const VisibilityBuffer::VisibilityPassData& VisibilityBuffer::addVisibilityBufferPasses(Render::GraphBuilder& graph, const Render::PassData::GBuffer& gBufferData, const Render::TextureSize& framebufferSize) {
        struct Empty {};
        auto& prePass = graph.addPass<Empty>("pre pass visibility buffer",
            [this](Render::GraphBuilder& builder, Render::Pass<Empty>& pass, Empty data) {
                pass.rasterized = false;
            },
            [this](const Render::CompiledPass& pass, const Render::Context& frame, const Empty& data, vk::CommandBuffer& cmds) {
                // clear readback buffer to ensure visible count starts at 0 for the new frame
                auto clearReadbackPipeline = renderer.getOrCreatePipelineFullPath("resources/pipelines/compute/clear-singleint.pipeline", (std::uint64_t)&pass);
                auto readbackBufferOpt = renderer.getMeshletManager().getReadbackBuffer(frame.pViewport, frame.swapchainIndex);
                if(readbackBufferOpt.hasValue()) {
                    vk::DeviceAddress addr = readbackBufferOpt->getWholeView().getDeviceAddress() + offsetof(ClusterReadbackData, visibleCount);
                    renderer.pushConstantBlock("address", *clearReadbackPipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, addr);

                    clearReadbackPipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);
                    cmds.dispatch(1, 1, 1);
                }

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

                // Record all render packets (~= draw commands) with the tag "PrePassVisibilityBuffer" then "VisibilityBuffer"
                {
                    GPUZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Pre pass visibility buffer");
                    frame.renderer.recordPassPackets(Render::PassEnum::PrePassVisibilityBuffer, pass, frame, cmds);
                }
            });

        auto& clearPass = graph.addPass<VisibilityBufferRasterizationData>("clear visibility buffer",
            [this, &gBufferData, framebufferSize](Render::GraphBuilder& builder, Render::Pass<VisibilityBufferRasterizationData>& pass, VisibilityBufferRasterizationData& data) {
                        // Declare the visibility buffer texture
                        data.visibilityBuffer = builder.createStorageTarget("visibility buffer", vk::Format::eR64Uint, framebufferSize, vk::ImageLayout::eGeneral);
                        pass.rasterized = false;
                    },
                    [this](const Render::CompiledPass& pass, const Render::Context& frame, const VisibilityBufferRasterizationData& data, vk::CommandBuffer& cmds) {
                        ZoneScopedN("CPU RenderGraph visibility buffer clear");
                        GPUZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "clear buffer rasterize");
                        auto& texture = pass.getGraph().getTexture(data.visibilityBuffer, frame.swapchainIndex);

                        // clear visibility buffer to reset depth
                        auto clearBufferPipeline = renderer.getOrCreatePipelineFullPath("resources/pipelines/compute/clear-visibility-buffer.pipeline", (std::uint64_t)&pass);
                        renderer.bindStorageImage(*clearBufferPipeline, frame, texture, 0, 0,
                                                  vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                        const auto& extent = texture.getSize();
                        const std::uint8_t localSize = 32;
                        std::size_t dispatchX = (extent.width + (localSize-1)) / localSize;
                        std::size_t dispatchY = (extent.height + (localSize-1)) / localSize;

                        clearBufferPipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);
                        cmds.dispatch(dispatchX, dispatchY, 1);
                    });
        // Add the hardware rasterization pass
        auto& rasterizePass = graph.addPass<VisibilityBufferRasterizationData>("rasterize visibility buffer",
            [this, &clearPass, framebufferSize](Render::GraphBuilder& builder, Render::Pass<VisibilityBufferRasterizationData>& pass, VisibilityBufferRasterizationData& data) {
                // Declare the visibility buffer texture
                data.visibilityBuffer = builder.write(clearPass.getData().visibilityBuffer, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eGeneral);
            },
            [this](const Render::CompiledPass& pass, const Render::Context& frame, const VisibilityBufferRasterizationData& data, vk::CommandBuffer& cmds) {
                ZoneScopedN("CPU RenderGraph visibility buffer rasterize");
                GPUZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "visibility buffer rasterize");
                auto& texture = pass.getGraph().getTexture(data.visibilityBuffer, frame.swapchainIndex);

                // instanceIndex must match with one used in MeshletManager to reference the proper pipeline
                auto pipeline = renderer.getOrCreatePipelineFullPath("resources/pipelines/visibility-buffer.pipeline", (std::uint64_t)frame.pViewport);
                renderer.bindStorageImage(*pipeline, frame, texture, 0, 3,
                                          vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                {
                    GPUZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Visibility buffer");
                    frame.renderer.recordPassPackets(Render::PassEnum::VisibilityBuffer, pass, frame, cmds);
                }
            }
        );

        auto addDebugPass = [&](const std::string& shaderNameSuffix, int renderDebugType) {
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
                   GetEngine().getVulkanDriver().getResourceRepository().getTextureUsages(data.postProcessed.rootID) |= vk::ImageUsageFlagBits::eSampled;
               },
               [this, pipelineName = Carrot::sprintf("post-process/visibility-buffer-debug/%s", shaderNameSuffix.c_str())]
               (const Render::CompiledPass& pass, const Render::Context& frame, const PassData::PostProcessing& data, vk::CommandBuffer& cmds) {
                   ZoneScopedN("CPU RenderGraph visibility buffer debug view");
                   GPUZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "visibility buffer debug view");
                   auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline(pipelineName, pass);

                   Carrot::BufferView clusters = frame.renderer.getMeshletManager().getClusters(frame);
                   Carrot::BufferView clusterInstances = frame.renderer.getMeshletManager().getClusterInstances(frame);
                   Carrot::BufferView clusterInstanceData = frame.renderer.getMeshletManager().getClusterInstanceData(frame);
                   if(!clusters || !clusterInstances || !clusterInstanceData) {
                       return;
                   }

                   const auto& visibilityBufferTexture = pass.getGraph().getTexture(data.postLighting, frame.swapchainIndex);
                   frame.renderer.getMaterialSystem().bind(frame, cmds, 0, pipeline->getPipelineLayout());
                   frame.renderer.bindStorageImage(*pipeline, frame, visibilityBufferTexture, 1, 0,
                       vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                   frame.renderer.bindBuffer(*pipeline, frame, clusters, 1, 1);
                   frame.renderer.bindBuffer(*pipeline, frame, clusterInstances, 1, 2);
                   frame.renderer.bindBuffer(*pipeline, frame, clusterInstanceData, 1, 3);

                   pipeline->bind(pass, frame, cmds);
                   auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                   screenQuadMesh.bind(cmds);
                   screenQuadMesh.draw(cmds);
               }
            );
            debugPass.setCondition([this, renderDebugType](const CompiledPass&, const Render::Context&, const PassData::PostProcessing&) {
                return GetRenderer().getDebugRenderType() == renderDebugType;
            });

            return debugPass.getData().postProcessed;
        };
        std::array<Render::FrameResource, DEBUG_VISIBILITY_BUFFER_LAST - DEBUG_VISIBILITY_BUFFER_FIRST +1> debugViews;
        debugViews[DEBUG_VISIBILITY_BUFFER_TRIANGLES - DEBUG_VISIBILITY_BUFFER_FIRST] = addDebugPass("triangles", DEBUG_VISIBILITY_BUFFER_TRIANGLES);
        debugViews[DEBUG_VISIBILITY_BUFFER_CLUSTERS - DEBUG_VISIBILITY_BUFFER_FIRST] = addDebugPass("clusters", DEBUG_VISIBILITY_BUFFER_CLUSTERS);
        debugViews[DEBUG_VISIBILITY_BUFFER_LODS - DEBUG_VISIBILITY_BUFFER_FIRST] = addDebugPass("lods", DEBUG_VISIBILITY_BUFFER_LODS);
        debugViews[DEBUG_VISIBILITY_BUFFER_SCREEN_ERROR - DEBUG_VISIBILITY_BUFFER_FIRST] = addDebugPass("screen-error", DEBUG_VISIBILITY_BUFFER_SCREEN_ERROR);

        // Take visibility buffer as input and render materials to GBuffer
        auto& materialPass = graph.addPass<VisibilityPassData>("visibility buffer materials",
            [this, &gBufferData, &rasterizePass, &debugViews](Render::GraphBuilder& builder, Render::Pass<VisibilityPassData>& pass, VisibilityPassData& data) {
                data.gbuffer.writeTo(builder, gBufferData);
                data.visibilityBuffer = builder.read(rasterizePass.getData().visibilityBuffer, vk::ImageLayout::eGeneral);
                for(int i = DEBUG_VISIBILITY_BUFFER_FIRST; i <= DEBUG_VISIBILITY_BUFFER_LAST; i++) {
                    int debugIndex = i - DEBUG_VISIBILITY_BUFFER_FIRST;
                    data.debugViews[debugIndex] = debugViews[debugIndex];
                }
            },
            [this](const Render::CompiledPass& pass, const Render::Context& frame, const VisibilityPassData& data, vk::CommandBuffer& cmds) {
                Carrot::BufferView clusters = frame.renderer.getMeshletManager().getClusters(frame);
                Carrot::BufferView clusterInstances = frame.renderer.getMeshletManager().getClusterInstances(frame);
                Carrot::BufferView clusterInstanceData = frame.renderer.getMeshletManager().getClusterInstanceData(frame);
                if(!clusters || !clusterInstances || !clusterInstanceData) {
                    return;
                }

                auto pipeline = frame.renderer.getOrCreateRenderPassSpecificPipeline("resources/pipelines/material-pass.pipeline", pass);
                const auto& visibilityBufferTexture = pass.getGraph().getTexture(data.visibilityBuffer, frame.swapchainIndex);
                data.gbuffer.bindInputs(*pipeline, frame, pass.getGraph(), 0, vk::ImageLayout::eColorAttachmentOptimal);
                frame.renderer.getMaterialSystem().bind(frame, cmds, 1, pipeline->getPipelineLayout());
                frame.renderer.bindStorageImage(*pipeline, frame, visibilityBufferTexture, 2, 0,
                    vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                frame.renderer.bindBuffer(*pipeline, frame, clusters, 2, 1);
                frame.renderer.bindBuffer(*pipeline, frame, clusterInstances, 2, 2);
                frame.renderer.bindBuffer(*pipeline, frame, clusterInstanceData, 2, 3);

                pipeline->bind(pass, frame, cmds);
                auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                screenQuadMesh.bind(cmds);
                screenQuadMesh.draw(cmds);
            }
        );

        return materialPass.getData();
    }
} // Carrot::Render