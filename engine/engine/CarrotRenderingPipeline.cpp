//
// Created by jglrxavpok on 10/12/2022.
//

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "engine/Engine.h"
#include <algorithm>
#include "engine/constants.h"
#include "engine/render/resources/Image.h"
#include "engine/render/resources/SingleMesh.h"
#include "engine/render/raytracing/RayTracer.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/render/GBuffer.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/render/resources/Texture.h"
#include "engine/CarrotGame.h"
#include "stb_image_write.h"
#include "engine/render/RenderGraph.h"
#include "engine/render/TextureRepository.h"
#include "core/Macros.h"
#include "engine/render/ComputePipeline.h"

Carrot::Render::Pass<Carrot::Render::PassData::PostProcessing>& Carrot::Engine::fillInDefaultPipeline(Carrot::Render::GraphBuilder& mainGraph, Carrot::Render::Eye eye,
                                                                                                      std::function<void(const Carrot::Render::CompiledPass&,
                                                                                                                         const Carrot::Render::Context&,
                                                                                                                         vk::CommandBuffer&)> opaqueCallback,
                                                                                                      std::function<void(const Carrot::Render::CompiledPass&,
                                                                                                                         const Carrot::Render::Context&,
                                                                                                                         vk::CommandBuffer&)> transparentCallback,
                                                                                                      const Render::TextureSize& framebufferSize) {

    auto& opaqueGBufferPass = getGBuffer().addGBufferPass(mainGraph, [opaqueCallback](const Render::CompiledPass& pass, const Render::Context& frame, vk::CommandBuffer& cmds) {
        ZoneScopedN("CPU RenderGraph Opaque GPass");
        opaqueCallback(pass, frame, cmds);
    }, framebufferSize);

    auto& lightingPass = getGBuffer().addLightingPass(opaqueGBufferPass.getData(), mainGraph, framebufferSize);

    struct Denoising {
        Render::FrameResource beauty;
        Render::PassData::GBuffer gBufferInput;
        Render::FrameResource momentsHistoryHistoryLength; // vec4(moment, moment², history length, __unused__)
        Render::FrameResource denoisedResult;
    };
    auto& temporalAccumulationPass = mainGraph.addPass<Denoising>(
            "temporal-denoise",
            [this, lightingPass, framebufferSize](Render::GraphBuilder& builder, Render::Pass<Denoising>& pass, Denoising& data) {
                data.gBufferInput.readFrom(builder, lightingPass.getData().gBuffer);
                data.beauty = builder.read(lightingPass.getData().resolved, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.denoisedResult = builder.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                                 data.beauty.size,
                                                                 vk::AttachmentLoadOp::eClear,
                                                                 vk::ClearColorValue(std::array{0,0,0,0}),
                                                                 vk::ImageLayout::eGeneral // TODO: color attachment?
                );
                data.momentsHistoryHistoryLength = builder.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                                              data.beauty.size,
                                                                              vk::AttachmentLoadOp::eClear,
                                                                              vk::ClearColorValue(std::array{0,0,0,0}),
                                                                              vk::ImageLayout::eGeneral // TODO: color attachment?
                );
            },
            [this](const Render::CompiledPass& pass, const Render::Context& frame, const Denoising& data, vk::CommandBuffer& buffer) {
                ZoneScopedN("CPU RenderGraph temporal-denoise");
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], buffer, "temporal-denoise");
                auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("post-process/temporal-denoise", pass.getRenderPass());
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.beauty, frame.swapchainIndex), 0, 0, nullptr);

                auto bindLastFrameTexture = [&](const Render::FrameResource& resource, std::uint32_t bindingIndex) {
                    Render::Texture* lastFrameTexture = nullptr;
                    if(frame.lastSwapchainIndex >= 0) {
                        lastFrameTexture = &pass.getGraph().getTexture(resource, frame.lastSwapchainIndex);
                    } else {
                        lastFrameTexture = GetRenderer().getMaterialSystem().getBlackTexture()->texture.get();
                    }
                    renderer.bindTexture(*pipeline, frame, *lastFrameTexture, 0, bindingIndex, nullptr);
                };
                bindLastFrameTexture(data.denoisedResult, 1);

                Render::Texture& viewPosTexture = pass.getGraph().getTexture(data.gBufferInput.positions, frame.swapchainIndex);
                renderer.bindTexture(*pipeline, frame, viewPosTexture, 0, 2, nullptr);
                bindLastFrameTexture(data.gBufferInput.positions, 3);

                bindLastFrameTexture(data.momentsHistoryHistoryLength, 5);

                renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getNearestSampler(), 0, 6);
                renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getLinearSampler(), 0, 7);

                renderer.bindUniformBuffer(*pipeline, frame, frame.viewport.getCameraUniformBuffer(frame), 1, 0);
                renderer.bindUniformBuffer(*pipeline, frame, frame.viewport.getCameraUniformBuffer(frame.lastFrame()), 1, 1);

                data.gBufferInput.bindInputs(*pipeline, frame, pass.getGraph(), 2);

                pipeline->bind(pass.getRenderPass(), frame, buffer);
                auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                screenQuadMesh.bind(buffer);
                screenQuadMesh.draw(buffer);
            }
    );

    struct SpatialDenoise {
        Render::FrameResource postTemporal;
        Render::PassData::GBuffer gBuffer;
        Render::FrameResource denoisedPingPong[2];
        Render::FrameResource momentsHistoryHistoryLength;

        std::uint8_t iterationCount = 0;
    };
    auto& spatialDenoise = mainGraph.addPass<SpatialDenoise>(
            "spatialDenoise",

            [this, temporalAccumulationPass, lightingPass](Render::GraphBuilder& builder, Render::Pass<SpatialDenoise>& pass, SpatialDenoise& data) {
                data.postTemporal = builder.read(temporalAccumulationPass.getData().denoisedResult, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.gBuffer.readFrom(builder, lightingPass.getData().gBuffer);

                for (int j = 0; j < 2; ++j) {
                    data.denoisedPingPong[j] = builder.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                                          data.postTemporal.size,
                                                                          vk::AttachmentLoadOp::eClear,
                                                                          vk::ClearColorValue(std::array{0,0,0,0}),
                                                                          vk::ImageLayout::eGeneral
                    );
                }
                data.momentsHistoryHistoryLength = builder.read(temporalAccumulationPass.getData().momentsHistoryHistoryLength, vk::ImageLayout::eShaderReadOnlyOptimal);

                data.iterationCount = 4;
                pass.rasterized = false; // compute pass
                pass.prerecordable = false; // TODO: why?
            },
            [this, framebufferSize](const Render::CompiledPass& pass, const Render::Context& frame, const SpatialDenoise& data, vk::CommandBuffer& buffer) {
                ZoneScopedN("CPU RenderGraph spatial denoise");
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], buffer, "spatial denoise");


                auto& momentsHistoryHistoryLengthTexture = pass.getGraph().getTexture(data.momentsHistoryHistoryLength, frame.swapchainIndex);
                auto& temporalTexture = pass.getGraph().getTexture(data.postTemporal, frame.swapchainIndex);
                auto& denoisedResultTexture0 = pass.getGraph().getTexture(data.denoisedPingPong[0], frame.swapchainIndex);
                auto& denoisedResultTexture1 = pass.getGraph().getTexture(data.denoisedPingPong[1], frame.swapchainIndex);

                const auto& extent = denoisedResultTexture0.getSize();

                std::shared_ptr<Pipeline> pipelinePingPong[3] = {
                        frame.renderer.getOrCreatePipeline("compute/spatial-denoise", 0u + ((std::uint64_t)&frame.viewport)),
                        frame.renderer.getOrCreatePipeline("compute/spatial-denoise", 1u + ((std::uint64_t)&frame.viewport)),
                        frame.renderer.getOrCreatePipeline("compute/spatial-denoise", 2u + ((std::uint64_t)&frame.viewport)),
                };

                data.gBuffer.bindInputs(*pipelinePingPong[0], frame, pass.getGraph(), 0);
                frame.renderer.bindStorageImage(*pipelinePingPong[0], frame, temporalTexture, 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                frame.renderer.bindStorageImage(*pipelinePingPong[0], frame, denoisedResultTexture0, 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                frame.renderer.bindStorageImage(*pipelinePingPong[0], frame, momentsHistoryHistoryLengthTexture, 1, 2, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);

                data.gBuffer.bindInputs(*pipelinePingPong[1], frame, pass.getGraph(), 0);
                frame.renderer.bindStorageImage(*pipelinePingPong[1], frame, denoisedResultTexture0, 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                frame.renderer.bindStorageImage(*pipelinePingPong[1], frame, denoisedResultTexture1, 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                frame.renderer.bindStorageImage(*pipelinePingPong[1], frame, momentsHistoryHistoryLengthTexture, 1, 2, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);

                data.gBuffer.bindInputs(*pipelinePingPong[2], frame, pass.getGraph(), 0);
                frame.renderer.bindStorageImage(*pipelinePingPong[2], frame, denoisedResultTexture1, 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                frame.renderer.bindStorageImage(*pipelinePingPong[2], frame, denoisedResultTexture0, 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                frame.renderer.bindStorageImage(*pipelinePingPong[2], frame, momentsHistoryHistoryLengthTexture, 1, 2, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);

                struct IterationData {
                    std::uint32_t index = 0;
                } iterationData;

                constexpr std::size_t localSize = 22;
                std::size_t dispatchX = (extent.width + (localSize-1)) / localSize;
                std::size_t dispatchY = (extent.height + (localSize-1)) / localSize;
                pipelinePingPong[0]->bind({}, frame, buffer, vk::PipelineBindPoint::eCompute);
                frame.renderer.pushConstantBlock("iterationData", *pipelinePingPong[0], frame, vk::ShaderStageFlagBits::eCompute, buffer, iterationData);
                buffer.dispatch(dispatchX, dispatchY, 1);

                for (std::uint8_t j = 0; j < data.iterationCount - 1; ++j) {
                    vk::MemoryBarrier2KHR memoryBarrier {
                        .srcStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                        .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                        .dstStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                        .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead,
                    };
                    vk::DependencyInfoKHR dependencyInfo {
                        .memoryBarrierCount = 1,
                        .pMemoryBarriers = &memoryBarrier,
                    };
                    buffer.pipelineBarrier2KHR(dependencyInfo);

                    auto pipeline = pipelinePingPong[j % 2 + 1];
                    pipeline->bind({}, frame, buffer, vk::PipelineBindPoint::eCompute);
                    iterationData.index++;
                    frame.renderer.pushConstantBlock("iterationData", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, buffer, iterationData);
                    buffer.dispatch(dispatchX, dispatchY, 1);
                }
            }
    );

    struct LightingMerge {
        Carrot::Render::PassData::GBuffer gBuffer;
        Render::FrameResource noisyLighting; // for debug
        Render::FrameResource lighting;
        Render::FrameResource momentsHistoryHistoryLength;
        Render::FrameResource mergeResult;
    };

    auto& mergeLighting = mainGraph.addPass<LightingMerge>(
            "merge-lighting",

            [this, lightingPass, temporalAccumulationPass, spatialDenoise, framebufferSize](Render::GraphBuilder& builder, Render::Pass<LightingMerge>& pass, LightingMerge& data) {
                data.gBuffer.readFrom(builder, lightingPass.getData().gBuffer);
                data.noisyLighting = builder.read(lightingPass.getData().resolved, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.lighting = builder.read(spatialDenoise.getData().denoisedPingPong[(spatialDenoise.getData().iterationCount + 1) % 2], vk::ImageLayout::eShaderReadOnlyOptimal);
                data.momentsHistoryHistoryLength = builder.read(temporalAccumulationPass.getData().momentsHistoryHistoryLength, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.mergeResult = builder.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                              framebufferSize,
                                                              vk::AttachmentLoadOp::eClear,
                                                              vk::ClearColorValue(std::array{0,0,0,0}),
                                                              vk::ImageLayout::eColorAttachmentOptimal
                );
            },
            [framebufferSize, this](const Render::CompiledPass& pass, const Render::Context& frame, const LightingMerge& data, vk::CommandBuffer& buffer) {
                ZoneScopedN("CPU RenderGraph merge-lighting");
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], buffer, "merge-lighting");
                auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("post-process/merge-lighting", pass.getRenderPass());

                struct PushConstant {
                    std::uint32_t frameCount;
                    std::uint32_t frameWidth;
                    std::uint32_t frameHeight;
                } block;

                block.frameCount = renderer.getFrameCount();
                if(framebufferSize.type == Render::TextureSize::Type::SwapchainProportional) {
                    block.frameWidth = framebufferSize.width * GetVulkanDriver().getWindowFramebufferExtent().width;
                    block.frameHeight = framebufferSize.height * GetVulkanDriver().getWindowFramebufferExtent().height;
                } else {
                    block.frameWidth = framebufferSize.width;
                    block.frameHeight = framebufferSize.height;
                }
                renderer.pushConstantBlock("push", *pipeline, frame, vk::ShaderStageFlagBits::eFragment, buffer, block);

                data.gBuffer.bindInputs(*pipeline, frame, pass.getGraph(), 0);
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.lighting, frame.swapchainIndex), 1, 0, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral); // TODO shader read only layout?
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.momentsHistoryHistoryLength, frame.swapchainIndex), 1, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral); // TODO shader read only layout?
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.noisyLighting, frame.swapchainIndex), 1, 2, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral); // TODO shader read only layout?

                pipeline->bind(pass.getRenderPass(), frame, buffer);
                auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                screenQuadMesh.bind(buffer);
                screenQuadMesh.draw(buffer);
            }
    );

    auto& toneMapping = mainGraph.addPass<Carrot::Render::PassData::PostProcessing>(
            "tone-mapping",

            [this, mergeLighting, framebufferSize](Render::GraphBuilder& builder, Render::Pass<Carrot::Render::PassData::PostProcessing>& pass, Carrot::Render::PassData::PostProcessing& data) {
                data.postLighting = builder.read(mergeLighting.getData().mergeResult, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.postProcessed = builder.createRenderTarget(vk::Format::eR8G8B8A8Srgb,
                                                                framebufferSize,
                                                                vk::AttachmentLoadOp::eClear,
                                                                vk::ClearColorValue(std::array{0,0,0,0}),
                                                                vk::ImageLayout::eColorAttachmentOptimal
                );
            },
            [this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::PostProcessing& data, vk::CommandBuffer& buffer) {
                ZoneScopedN("CPU RenderGraph post-process");
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], buffer, "Post-Process");
                auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("post-process/tone-mapping", pass.getRenderPass());
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.postLighting, frame.swapchainIndex), 0, 0, nullptr);

                renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getNearestSampler(), 0, 1);
                renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getLinearSampler(), 0, 2);

                pipeline->bind(pass.getRenderPass(), frame, buffer);
                auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                screenQuadMesh.bind(buffer);
                screenQuadMesh.draw(buffer);
            }
    );

    return toneMapping;
}

Carrot::Render::Pass<Carrot::Render::PassData::PostProcessing>& Carrot::Engine::fillGraphBuilder(Render::GraphBuilder& mainGraph, Render::Eye eye, const Render::TextureSize& framebufferSize) {
    auto& finalPass = fillInDefaultPipeline(mainGraph, eye,
                                            [&](const Render::CompiledPass& pass, const Render::Context& frame, vk::CommandBuffer& cmds) {
                                                TracyVkZone(tracyCtx[frame.swapchainIndex], cmds, "Opaque Rendering");
                                                ZoneScopedN("CPU RenderGraph Opaque GPass");
                                                game->recordOpaqueGBufferPass(pass.getRenderPass(), frame, cmds);
                                                renderer.recordOpaqueGBufferPass(pass.getRenderPass(), frame, cmds);
                                            },
                                            [&](const Render::CompiledPass& pass, const Render::Context& frame, vk::CommandBuffer& cmds) {
                                                TracyVkZone(tracyCtx[frame.swapchainIndex], cmds, "Transparent Rendering");
                                                ZoneScopedN("CPU RenderGraph Transparent GPass");
                                                game->recordTransparentGBufferPass(pass.getRenderPass(), frame, cmds);
                                                renderer.recordTransparentGBufferPass(pass.getRenderPass(), frame, cmds);
                                            },
                                            framebufferSize);

    return finalPass;
}