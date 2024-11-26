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
#include "engine/render/VisibilityBuffer.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/render/resources/Texture.h"
#include "engine/CarrotGame.h"
#include "stb_image_write.h"
#include "engine/render/RenderGraph.h"
#include "engine/render/TextureRepository.h"
#include "core/Macros.h"
#include "engine/render/ComputePipeline.h"
#include "render/DebugBufferObject.h"

const Carrot::Render::FrameResource& Carrot::Engine::fillInDefaultPipeline(Carrot::Render::GraphBuilder& mainGraph, Carrot::Render::Eye eye,
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

    auto& visibilityPasses = getVisibilityBuffer().addVisibilityBufferPasses(mainGraph, opaqueGBufferPass.getData(), framebufferSize);

    const float scaleFactor = 0.75f;
    Render::TextureSize lightingFramebufferSize;
    lightingFramebufferSize.type = framebufferSize.type;
    lightingFramebufferSize.width = scaleFactor * framebufferSize.width;
    lightingFramebufferSize.height = scaleFactor * framebufferSize.height;

    auto& lightingPass = getGBuffer().addLightingPass(visibilityPasses.gbuffer, mainGraph, lightingFramebufferSize);

    struct DenoisingResult {
        Render::FrameResource input;
        Render::FrameResource denoiseResult;
    };

    struct TemporalAccumulation {
        Render::FrameResource beauty;
        Render::PassData::GBuffer gBufferInput;
        Render::FrameResource momentsHistoryHistoryLength; // vec4(moment, moment², history length, __unused__)
        Render::FrameResource denoisedResult;
        Render::FrameResource firstSpatialDenoiseColor;
        Render::FrameResource viewPositions;
    };

    auto makeTAAPass = [&](std::string_view name, const Render::FrameResource& toAntiAlias, const Render::PassData::GBuffer& gBuffer,
        const Render::FrameResource& viewPositions,
        const Render::TextureSize& textureSize) -> Render::Pass<TemporalAccumulation>& {
        auto& temporalAccumulationPass = mainGraph.addPass<TemporalAccumulation>(
                "temporal anti aliasing - " + std::string(name),
                [this, toAntiAlias, framebufferSize, textureSize, gBuffer, viewPositions](Render::GraphBuilder& builder, Render::Pass<TemporalAccumulation>& pass, TemporalAccumulation& data) {
                    data.gBufferInput.readFrom(builder, gBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
                    data.beauty = builder.read(toAntiAlias, vk::ImageLayout::eShaderReadOnlyOptimal);
                    data.denoisedResult = builder.createRenderTarget("Temporal accumulation",
                                                                     toAntiAlias.format,
                                                                     textureSize,
                                                                     vk::AttachmentLoadOp::eClear,
                                                                     vk::ClearColorValue(std::array{0,0,0,0}),
                                                                     vk::ImageLayout::eGeneral // TODO: color attachment?
                    );

                    data.viewPositions = builder.read(viewPositions, vk::ImageLayout::eShaderReadOnlyOptimal);

                    data.momentsHistoryHistoryLength = builder.createRenderTarget("Moments & history length",
                                                                                  vk::Format::eR32G32B32A32Sfloat,
                                                                                  data.denoisedResult.size,
                                                                                  vk::AttachmentLoadOp::eClear,
                                                                                  vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,1.0f}),
                                                                                  vk::ImageLayout::eGeneral // TODO: color attachment?
                    );

                    data.firstSpatialDenoiseColor = builder.createRenderTarget("First spatial denoise",
                                                                               vk::Format::eR32G32B32A32Sfloat,
                                                                               data.denoisedResult.size,
                                                                               vk::AttachmentLoadOp::eClear,
                                                                               vk::ClearColorValue(std::array{0,0,0,0}),
                                                                               vk::ImageLayout::eGeneral
                    );
                    GetVulkanDriver().getResourceRepository().getTextureUsages(data.momentsHistoryHistoryLength.rootID) |= vk::ImageUsageFlagBits::eSampled;
                    GetVulkanDriver().getResourceRepository().getTextureUsages(data.firstSpatialDenoiseColor.rootID) |= vk::ImageUsageFlagBits::eSampled;
                },
                [this](const Render::CompiledPass& pass, const Render::Context& frame, const TemporalAccumulation& data, vk::CommandBuffer& buffer) {
                    ZoneScopedN("CPU RenderGraph temporal-denoise");
                    TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], buffer, "temporal-denoise");
                    auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("post-process/temporal-denoise", pass.getRenderPass());
                    renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.beauty, frame.swapchainIndex), 0, 0, nullptr);

                    auto bindLastFrameTexture = [&](const Render::FrameResource& resource, std::uint32_t bindingIndex, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal) {
                        Render::Texture* lastFrameTexture = nullptr;
                        if(frame.lastSwapchainIndex >= 0) {
                            lastFrameTexture = &pass.getGraph().getTexture(resource, frame.lastSwapchainIndex);
                        } else {
                            lastFrameTexture = GetRenderer().getMaterialSystem().getBlackTexture()->texture.get();
                        }
                        renderer.bindTexture(*pipeline, frame, *lastFrameTexture, 0, bindingIndex, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, layout);
                    };
                    bindLastFrameTexture(data.denoisedResult, 1, vk::ImageLayout::eGeneral);

                    Render::Texture& viewPosTexture = pass.getGraph().getTexture(data.viewPositions, frame.swapchainIndex);
                    renderer.bindTexture(*pipeline, frame, viewPosTexture, 0, 2, nullptr);
                    bindLastFrameTexture(data.viewPositions, 3);

                    bindLastFrameTexture(data.momentsHistoryHistoryLength, 5, vk::ImageLayout::eGeneral);

                    renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getNearestSampler(), 0, 6);
                    renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getLinearSampler(), 0, 7);

                    renderer.bindUniformBuffer(*pipeline, frame, frame.pViewport->getCameraUniformBuffer(frame), 1, 0);
                    renderer.bindUniformBuffer(*pipeline, frame, frame.pViewport->getCameraUniformBuffer(frame.lastFrame()), 1, 1);

                    data.gBufferInput.bindInputs(*pipeline, frame, pass.getGraph(), 2, vk::ImageLayout::eShaderReadOnlyOptimal);

                    pipeline->bind(pass.getRenderPass(), frame, buffer);
                    auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                    screenQuadMesh.bind(buffer);
                    screenQuadMesh.draw(buffer);
                }
        );
        return temporalAccumulationPass;
    };

    // TODO: move to lighting pass?
#if 0
    auto denoise = [&](const Render::PassData::Lighting& input) -> DenoisingResult {
        if(!GetCapabilities().supportsRaytracing) {
            // no-op
            DenoisingResult result;
            result.input = input.globalIllumination;
            result.denoiseResult = input.globalIllumination;
            return result;
        }


        auto& temporalAccumulationPass = makeTAAPass("lighting", input.globalIllumination, lightingPass.getData().gBuffer, input.firstBouncePositions, /*lightingFramebufferSize*/framebufferSize,
            false);

        struct FireflyRejection {
            Render::FrameResource temporalAccumulation;
            Render::FrameResource output;
        };

        auto& fireflyRejectionPass = mainGraph.addPass<FireflyRejection>(
                "firefly-rejection - " + input.globalIllumination.name,
                [this, &input, temporalAccumulationPass](Render::GraphBuilder& builder, Render::Pass<FireflyRejection>& pass, FireflyRejection& data) {
                    data.temporalAccumulation = builder.read(temporalAccumulationPass.getData().denoisedResult, vk::ImageLayout::eGeneral);
                    data.output = builder.createRenderTarget(
                            "Firefly result",
                            data.temporalAccumulation.format,
                            data.temporalAccumulation.size,
                            vk::AttachmentLoadOp::eClear,
                            vk::ClearColorValue(std::array{0,0,0,0}),
                            vk::ImageLayout::eGeneral
                    );

                    pass.rasterized = false;
                    pass.prerecordable = false;
                },
                [this](const Render::CompiledPass& pass, const Render::Context& frame, const FireflyRejection& data, vk::CommandBuffer& cmds) {
                    auto& inputTexture = pass.getGraph().getTexture(data.temporalAccumulation, frame.swapchainIndex);

                    {
                        vk::ImageMemoryBarrier2KHR imageMemoryBarrier = {
                                .srcStageMask = vk::PipelineStageFlagBits2KHR::eColorAttachmentOutput,
                                .srcAccessMask = vk::AccessFlagBits2KHR::eColorAttachmentWrite,
                                .dstStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                                .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead,
                                .oldLayout = vk::ImageLayout::eGeneral,
                                .newLayout = vk::ImageLayout::eGeneral,

                                .image = inputTexture.getVulkanImage(),
                                .subresourceRange = vk::ImageSubresourceRange {
                                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                                        .baseMipLevel = 0,
                                        .levelCount = 1,
                                        .baseArrayLayer = 0,
                                        .layerCount = inputTexture.getImage().getLayerCount(),
                                }
                        };
                        vk::DependencyInfoKHR dependencyInfo = {
                                .imageMemoryBarrierCount = 1,
                                .pImageMemoryBarriers = &imageMemoryBarrier
                        };
                        cmds.pipelineBarrier2KHR(dependencyInfo);
                    }

                    auto pipeline = frame.renderer.getOrCreatePipeline("compute/firefly-rejection", (std::uint64_t)&pass);

                    auto& outputTexture = pass.getGraph().getTexture(data.output, frame.swapchainIndex);

                    const auto& extent = inputTexture.getSize();
                    const std::uint8_t localSize = 32;
                    std::size_t dispatchX = (extent.width + (localSize-1)) / localSize;
                    std::size_t dispatchY = (extent.height + (localSize-1)) / localSize;

                    frame.renderer.bindStorageImage(*pipeline, frame, inputTexture, 0, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipeline, frame, outputTexture, 0, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                    pipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);
                    cmds.dispatch(dispatchX, dispatchY, 1);

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
                    cmds.pipelineBarrier2KHR(dependencyInfo);
                }
        );

        struct VarianceCompute {
            Render::FrameResource input;
            Render::FrameResource momentsHistory;
            Render::FrameResource varianceOutput;
        };

        auto& varianceComputePass = mainGraph.addPass<VarianceCompute>(
                "variance-compute - " + input.globalIllumination.name,
                [this, &input, fireflyRejectionPass, temporalAccumulationPass](Render::GraphBuilder& builder, Render::Pass<VarianceCompute>& pass, VarianceCompute& data) {
                    data.input = builder.read(fireflyRejectionPass.getData().output, vk::ImageLayout::eGeneral);
                    data.momentsHistory = builder.read(temporalAccumulationPass.getData().momentsHistoryHistoryLength, vk::ImageLayout::eGeneral);
                    data.varianceOutput = builder.createRenderTarget("Variance",
                                                                     vk::Format::eR32Sfloat,
                                                                     data.input.size,
                                                                     vk::AttachmentLoadOp::eClear,
                                                                     vk::ClearColorValue(std::array{0,0,0,0}),
                                                                     vk::ImageLayout::eGeneral);

                    pass.rasterized = false; // compute pass
                    pass.prerecordable = false;
                },
                [](const Render::CompiledPass& pass, const Render::Context& frame, const VarianceCompute& data, vk::CommandBuffer& cmds) {
                    auto copyPipeline = frame.renderer.getOrCreatePipeline("compute/copy-variance", (std::uint64_t)frame.pViewport + (std::uint64_t)&pass);

                    auto& inputTexture = pass.getGraph().getTexture(data.input, frame.swapchainIndex);
                    auto& inputMomentsTexture = pass.getGraph().getTexture(data.momentsHistory, frame.swapchainIndex);
                    auto& outputTexture = pass.getGraph().getTexture(data.varianceOutput, frame.swapchainIndex);

                    const auto& extent = inputTexture.getSize();
                    const std::uint8_t localSize = 32;
                    std::size_t dispatchX = (extent.width + (localSize-1)) / localSize;
                    std::size_t dispatchY = (extent.height + (localSize-1)) / localSize;

                    frame.renderer.bindStorageImage(*copyPipeline, frame, inputTexture, 0, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*copyPipeline, frame, inputMomentsTexture, 0, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*copyPipeline, frame, outputTexture, 0, 2, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                    copyPipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);
                    cmds.dispatch(dispatchX, dispatchY, 1);

                    vk::MemoryBarrier2KHR memoryBarrier {
                            .srcStageMask = vk::PipelineStageFlagBits2KHR::eFragmentShader,
                            .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                            .dstStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                            .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead,
                    };
                    vk::DependencyInfoKHR dependencyInfo {
                            .memoryBarrierCount = 1,
                            .pMemoryBarriers = &memoryBarrier,
                    };
                    cmds.pipelineBarrier2KHR(dependencyInfo);
                }
        );

        struct SpatialDenoise {
            Render::FrameResource postTemporal;
            Render::PassData::GBuffer gBuffer;
            Render::FrameResource firstBounceViewPosition;
            Render::FrameResource firstBounceViewNormal;
            Render::FrameResource firstDenoised;
            Render::FrameResource denoisedPingPong[2];
            Render::FrameResource variancePingPong[2];
            Render::FrameResource originalVariance;

            std::uint8_t iterationCount = 0;
        };
        auto& spatialDenoise = mainGraph.addPass<SpatialDenoise>(
                "spatial-denoise - " + input.globalIllumination.name,

                [this, fireflyRejectionPass, varianceComputePass, temporalAccumulationPass, lightingPass](Render::GraphBuilder& builder, Render::Pass<SpatialDenoise>& pass, SpatialDenoise& data) {
                    data.postTemporal = builder.read(fireflyRejectionPass.getData().output, vk::ImageLayout::eGeneral);
                    data.gBuffer.readFrom(builder, lightingPass.getData().gBuffer, vk::ImageLayout::eGeneral);

                    data.firstDenoised = temporalAccumulationPass.getData().firstSpatialDenoiseColor;

                    for (int j = 0; j < 2; ++j) {
                        data.denoisedPingPong[j] = builder.createRenderTarget("Denoised ping-pong",
                                                                              vk::Format::eR32G32B32A32Sfloat,
                                                                              data.postTemporal.size,
                                                                              vk::AttachmentLoadOp::eClear,
                                                                              vk::ClearColorValue(std::array{0,0,0,0}),
                                                                              vk::ImageLayout::eGeneral
                        );
                        data.variancePingPong[j] = builder.createRenderTarget("Variance ping-pong",
                                                                              vk::Format::eR32Sfloat,
                                                                              data.postTemporal.size,
                                                                              vk::AttachmentLoadOp::eClear,
                                                                              vk::ClearColorValue(std::array{0,0,0,0}),
                                                                              vk::ImageLayout::eGeneral
                        );
                    }
                    data.originalVariance = builder.read(varianceComputePass.getData().varianceOutput, vk::ImageLayout::eGeneral);
                    data.firstBounceViewPosition = builder.read(lightingPass.getData().firstBouncePositions, vk::ImageLayout::eShaderReadOnlyOptimal);
                    data.firstBounceViewNormal = builder.read(lightingPass.getData().firstBounceNormals, vk::ImageLayout::eShaderReadOnlyOptimal);

                    data.iterationCount = 4;
                    pass.rasterized = false; // compute pass
                    pass.prerecordable = false;
                },
                [this, &input, framebufferSize](const Render::CompiledPass& pass, const Render::Context& frame, const SpatialDenoise& data, vk::CommandBuffer& buffer) {
                    ZoneScopedN("CPU RenderGraph spatial denoise");
                    TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], buffer, "spatial denoise");

                    auto& originalVariance = pass.getGraph().getTexture(data.originalVariance, frame.swapchainIndex);
                    auto& temporalTexture = pass.getGraph().getTexture(data.postTemporal, frame.swapchainIndex);
                    auto& firstDenoisedColor = pass.getGraph().getTexture(data.firstDenoised, frame.swapchainIndex);
                    auto& firstBouncePositions = pass.getGraph().getTexture(data.firstBounceViewPosition, frame.swapchainIndex);
                    auto& firstBounceNormals = pass.getGraph().getTexture(data.firstBounceViewNormal, frame.swapchainIndex);
                    auto& denoisedResultTexture0 = pass.getGraph().getTexture(data.denoisedPingPong[0], frame.swapchainIndex);
                    auto& denoisedVarianceTexture0 = pass.getGraph().getTexture(data.variancePingPong[0], frame.swapchainIndex);
                    auto& denoisedResultTexture1 = pass.getGraph().getTexture(data.denoisedPingPong[1], frame.swapchainIndex);
                    auto& denoisedVarianceTexture1 = pass.getGraph().getTexture(data.variancePingPong[1], frame.swapchainIndex);

                    const auto& extent = denoisedResultTexture0.getSize();

                    std::shared_ptr<Pipeline> pipelinePingPong[4] = {
                            frame.renderer.getOrCreatePipeline("compute/spatial-denoise", 0u + ((std::uint64_t)frame.pViewport) + (std::uint64_t)&input),
                            frame.renderer.getOrCreatePipeline("compute/spatial-denoise", 1u + ((std::uint64_t)frame.pViewport) + (std::uint64_t)&input),
                            frame.renderer.getOrCreatePipeline("compute/spatial-denoise", 2u + ((std::uint64_t)frame.pViewport) + (std::uint64_t)&input),
                            frame.renderer.getOrCreatePipeline("compute/spatial-denoise", 3u + ((std::uint64_t)frame.pViewport) + (std::uint64_t)&input),
                    };

                    // first pass
                    // denoised texture is fed back to temporal accumulation
                    data.gBuffer.bindInputs(*pipelinePingPong[0], frame, pass.getGraph(), 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipelinePingPong[0], frame, temporalTexture, 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipelinePingPong[0], frame, originalVariance, 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                    frame.renderer.bindStorageImage(*pipelinePingPong[0], frame, firstDenoisedColor, 1, 2, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipelinePingPong[0], frame, denoisedVarianceTexture0, 1, 3, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                    frame.renderer.bindTexture(*pipelinePingPong[0], frame, firstBouncePositions, 1, 4, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                    frame.renderer.bindTexture(*pipelinePingPong[0], frame, firstBounceNormals, 1, 5, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);


                    data.gBuffer.bindInputs(*pipelinePingPong[1], frame, pass.getGraph(), 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipelinePingPong[1], frame, firstDenoisedColor, 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipelinePingPong[1], frame, denoisedVarianceTexture0, 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                    frame.renderer.bindStorageImage(*pipelinePingPong[1], frame, denoisedResultTexture0, 1, 2, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipelinePingPong[1], frame, denoisedVarianceTexture1, 1, 3, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                    frame.renderer.bindTexture(*pipelinePingPong[1], frame, firstBouncePositions, 1, 4, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                    frame.renderer.bindTexture(*pipelinePingPong[1], frame, firstBounceNormals, 1, 5, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);


                    data.gBuffer.bindInputs(*pipelinePingPong[2], frame, pass.getGraph(), 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipelinePingPong[2], frame, denoisedResultTexture0, 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipelinePingPong[2], frame, denoisedVarianceTexture1, 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                    frame.renderer.bindStorageImage(*pipelinePingPong[2], frame, denoisedResultTexture1, 1, 2, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipelinePingPong[2], frame, denoisedVarianceTexture0, 1, 3, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                    frame.renderer.bindTexture(*pipelinePingPong[2], frame, firstBouncePositions, 1, 4, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                    frame.renderer.bindTexture(*pipelinePingPong[2], frame, firstBounceNormals, 1, 5, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);


                    data.gBuffer.bindInputs(*pipelinePingPong[3], frame, pass.getGraph(), 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipelinePingPong[3], frame, denoisedResultTexture1, 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipelinePingPong[3], frame, denoisedVarianceTexture0, 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                    frame.renderer.bindStorageImage(*pipelinePingPong[3], frame, denoisedResultTexture0, 1, 2, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindStorageImage(*pipelinePingPong[3], frame, denoisedVarianceTexture1, 1, 3, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                    frame.renderer.bindTexture(*pipelinePingPong[3], frame, firstBouncePositions, 1, 4, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                    frame.renderer.bindTexture(*pipelinePingPong[3], frame, firstBounceNormals, 1, 5, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);

                    struct IterationData {
                        std::uint32_t index = 0;
                    } iterationData;

                    constexpr std::size_t localSizeX = 16;
                    constexpr std::size_t localSizeY = 8;
                    std::size_t dispatchX = (extent.width + (localSizeX-1)) / localSizeX;
                    std::size_t dispatchY = (extent.height + (localSizeY-1)) / localSizeY;

                    auto performSingleIteration = [&](std::size_t pipelineIndex) {
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

                        auto pipeline = pipelinePingPong[pipelineIndex];
                        pipeline->bind({}, frame, buffer, vk::PipelineBindPoint::eCompute);
                        frame.renderer.pushConstantBlock("iterationData", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, buffer, iterationData);
                        buffer.dispatch(dispatchX, dispatchY, 1);

                        iterationData.index++;
                    };

                    performSingleIteration(0);
                    performSingleIteration(1);

                    for (std::uint8_t j = 0; j < data.iterationCount - 2; ++j) {
                        performSingleIteration(j % 2 + 2);
                    }
                }
        );

        DenoisingResult result;
        result.input = input.globalIllumination;
        result.denoiseResult = spatialDenoise.getData().denoisedPingPong[(spatialDenoise.getData().iterationCount) % 2];
        return result;
    };
#endif

    struct LightingMerge {
        Carrot::Render::PassData::GBuffer gBuffer;
        Render::FrameResource directLighting;
        Render::FrameResource ambientOcclusion;
        Render::FrameResource reflections;
        Render::FrameResource visibilityBufferDebug[DEBUG_VISIBILITY_BUFFER_LAST - DEBUG_VISIBILITY_BUFFER_FIRST + 1];
        Render::FrameResource mergeResult;
    };

    auto& mergeLighting = mainGraph.addPass<LightingMerge>(
            "merge-lighting",

            [this, lightingPass, visibilityPasses, framebufferSize](Render::GraphBuilder& builder, Render::Pass<LightingMerge>& pass, LightingMerge& data) {
                data.gBuffer.readFrom(builder, lightingPass.getData().gBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.directLighting = builder.read(lightingPass.getData().directLighting.pingPong[(lightingPass.getData().directLighting.iterationCount+1) % 2], vk::ImageLayout::eShaderReadOnlyOptimal);
                data.ambientOcclusion = builder.read(lightingPass.getData().ambientOcclusion.pingPong[(lightingPass.getData().ambientOcclusion.iterationCount+1) % 2], vk::ImageLayout::eShaderReadOnlyOptimal);
                data.reflections = builder.read(lightingPass.getData().reflectionsNoisy, vk::ImageLayout::eShaderReadOnlyOptimal);

                for(int i = DEBUG_VISIBILITY_BUFFER_FIRST; i <= DEBUG_VISIBILITY_BUFFER_LAST; i++) {
                    int debugIndex = i - DEBUG_VISIBILITY_BUFFER_FIRST;
                    data.visibilityBufferDebug[debugIndex] = builder.read(visibilityPasses.debugViews[debugIndex], vk::ImageLayout::eShaderReadOnlyOptimal);
                }

                data.mergeResult = builder.createRenderTarget("Merged",
                                                              vk::Format::eR32G32B32A32Sfloat,
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
                    block.frameWidth = framebufferSize.width * frame.pViewport->getWidth();
                    block.frameHeight = framebufferSize.height * frame.pViewport->getHeight();
                } else {
                    block.frameWidth = framebufferSize.width;
                    block.frameHeight = framebufferSize.height;
                }
                renderer.pushConstantBlock("push", *pipeline, frame, vk::ShaderStageFlagBits::eFragment, buffer, block);

                data.gBuffer.bindInputs(*pipeline, frame, pass.getGraph(), 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.directLighting, frame.swapchainIndex), 1, 0, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.ambientOcclusion, frame.swapchainIndex), 1, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.reflections, frame.swapchainIndex), 1, 2, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);

                for(int i = DEBUG_VISIBILITY_BUFFER_FIRST; i <= DEBUG_VISIBILITY_BUFFER_LAST; i++) {
                    int debugIndex = i - DEBUG_VISIBILITY_BUFFER_FIRST;
                    renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.visibilityBufferDebug[debugIndex], frame.swapchainIndex), 1, 3, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, debugIndex, vk::ImageLayout::eShaderReadOnlyOptimal);
                }

                pipeline->bind(pass.getRenderPass(), frame, buffer);
                auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                screenQuadMesh.bind(buffer);
                screenQuadMesh.draw(buffer);
            }
    );

    struct UnlitDraw {
        Render::FrameResource depthBuffer;
        Render::FrameResource inout;
    };

    auto& drawUnlit = mainGraph.addPass<UnlitDraw>(
            "draw-unlit",
            [this, mergeLighting, framebufferSize](Render::GraphBuilder& builder, Render::Pass<UnlitDraw>& pass, UnlitDraw& data) {
                data.depthBuffer = builder.write(mergeLighting.getData().gBuffer.depthStencil, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
                data.inout = builder.write(mergeLighting.getData().mergeResult, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
            },
            [this](const Render::CompiledPass& pass, const Render::Context& frame, const UnlitDraw& data, vk::CommandBuffer& buffer) {
                ZoneScopedN("CPU RenderGraph draw-unlit");
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], buffer, "draw-unlit");
                GetRenderer().recordPassPackets(Render::PassEnum::Unlit, pass.getRenderPass(), frame, buffer);
            });

    auto& toneMapping = mainGraph.addPass<Carrot::Render::PassData::PostProcessing>(
            "tone-mapping",

            [this, drawUnlit, framebufferSize](Render::GraphBuilder& builder, Render::Pass<Carrot::Render::PassData::PostProcessing>& pass, Carrot::Render::PassData::PostProcessing& data) {
                data.postLighting = builder.read(drawUnlit.getData().inout, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.postProcessed = builder.createRenderTarget("Tone mapped",
                                                                vk::Format::eR8G8B8A8Srgb,
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


    auto& finalTAA = makeTAAPass("final", toneMapping.getData().postProcessed, lightingPass.getData().gBuffer, lightingPass.getData().gBuffer.positions, framebufferSize);

    return finalTAA.getData().denoisedResult;
}

const Carrot::Render::FrameResource& Carrot::Engine::fillGraphBuilder(Render::GraphBuilder& mainGraph, Render::Eye eye, const Render::TextureSize& framebufferSize) {
    return fillInDefaultPipeline(mainGraph, eye,
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

}