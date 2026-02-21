//
// Created by jglrxavpok on 10/12/2022.
//

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "engine/Engine.h"
#include <algorithm>
#include <core/math/BasicFunctions.h>

#include "engine/constants.h"
#include "engine/render/resources/Image.h"
#include "engine/render/resources/SingleMesh.h"
#include "engine/render/raytracing/RayTracer.h"
#include "engine/render/raytracing/RaytracingScene.h"
#include "engine/render/GBuffer.h"
#include "engine/render/VisibilityBuffer.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/render/resources/Texture.h"
#include "engine/CarrotGame.h"
#include "stb_image_write.h"
#include "console/RuntimeOption.hpp"
#include "engine/render/RenderGraph.h"
#include "engine/render/TextureRepository.h"
#include "core/Macros.h"
#include "engine/render/ComputePipeline.h"
#include "render/DebugBufferObject.h"
#include "render/lighting/LightingPasses.h"

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

    const float scaleFactor = 1.0f;
    Render::TextureSize lightingFramebufferSize;
    lightingFramebufferSize.type = framebufferSize.type;
    lightingFramebufferSize.width = scaleFactor * framebufferSize.width;
    lightingFramebufferSize.height = scaleFactor * framebufferSize.height;

    auto lightingData = Carrot::Render::addLightingPasses(visibilityPasses.gbuffer, mainGraph, lightingFramebufferSize);

    struct DenoisingResult {
        Render::FrameResource input;
        Render::FrameResource denoiseResult;
    };

    struct TemporalAccumulation {
        Render::FrameResource beauty;
        Render::PassData::GBuffer gBufferInput;
        Render::FrameResource momentsHistoryHistoryLength; // vec4(moment, moment², history length, __unused__)
        Render::FrameResource denoisedResult;
        Render::FrameResource viewPositions;
    };

    auto makeTAAPass = [&](std::string_view name, const Render::FrameResource& toAntiAlias, const Render::PassData::GBuffer& gBuffer,
        const Render::FrameResource& viewPositions,
        const Render::TextureSize& textureSize) -> Render::Pass<TemporalAccumulation>& {
        auto& temporalAccumulationPass = mainGraph.addPass<TemporalAccumulation>(
                "temporal anti aliasing - " + std::string(name),
                [this, toAntiAlias, framebufferSize, textureSize, gBuffer, viewPositions](Render::GraphBuilder& builder, Render::Pass<TemporalAccumulation>& pass, TemporalAccumulation& data) {
                    data.gBufferInput.readFrom(builder, gBuffer, vk::ImageLayout::eGeneral);
                    data.beauty = builder.read(toAntiAlias, vk::ImageLayout::eShaderReadOnlyOptimal);
                    data.denoisedResult = builder.createStorageTarget("Temporal accumulation",
                                                                     toAntiAlias.format,
                                                                     textureSize,
                                                                     vk::ImageLayout::eGeneral // TODO: color attachment?
                    );

                    builder.reuseResourceAcrossFrames(data.denoisedResult, 1); // used in temporal algorithms

                    data.viewPositions = builder.read(viewPositions, vk::ImageLayout::eGeneral);

                    data.momentsHistoryHistoryLength = builder.createStorageTarget("Moments & history length",
                                                                                  vk::Format::eR32G32B32A32Sfloat,
                                                                                  data.denoisedResult.size,
                                                                                  vk::ImageLayout::eGeneral // TODO: color attachment?
                    );

                    GetEngine().getResourceRepository().getTextureUsages(data.momentsHistoryHistoryLength.rootID) |= vk::ImageUsageFlagBits::eSampled;

                    builder.reuseResourceAcrossFrames(data.momentsHistoryHistoryLength, 1); // used in temporal algorithms

                    pass.rasterized = false;
                },
                [this](const Render::CompiledPass& pass, const Render::Context& frame, const TemporalAccumulation& data, vk::CommandBuffer& buffer) {
                    ZoneScopedN("CPU RenderGraph TAA");
                    GPUZone(GetEngine().tracyCtx[frame.frameIndex], buffer, "TAA");
                    auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("post-process/taa", pass);
                    auto& currentFrameColor = pass.getGraph().getTexture(data.beauty, frame.frameNumber);
                    pipeline->setStorageImage(frame, "io.output", pass.getGraph().getTexture(data.denoisedResult, frame.frameNumber), vk::ImageLayout::eGeneral);
                    pipeline->setSampledImage(frame, "io.currentFrameColor", currentFrameColor);

                    auto bindLastFrameTexture = [&](const Render::FrameResource& resource, const std::string& bindingSlot) {
                        Render::Texture& lastFrameTexture = pass.getGraph().getTexture(resource, frame.getPreviousFrameNumber());
                        pipeline->setSampledImage(frame, bindingSlot, lastFrameTexture);
                    };
                    bindLastFrameTexture(data.denoisedResult, "io.accumulationBuffer");

                    bindLastFrameTexture(data.momentsHistoryHistoryLength, "io.momentsAndHistory");

                    pipeline->setSampler(frame, "io.linearSampler", renderer.getVulkanDriver().getLinearSampler());
                    pipeline->setSampler(frame, "io.nearestSampler", renderer.getVulkanDriver().getNearestSampler());

                    data.gBufferInput.bindInputs(*pipeline, frame, pass.getGraph(), 0, vk::ImageLayout::eGeneral);

                    pipeline->bind(pass, frame, buffer, vk::PipelineBindPoint::eCompute);
                    const u32 groupX = Carrot::Math::alignUp(currentFrameColor.getSize().width, (u32)8);
                    const u32 groupY = Carrot::Math::alignUp(currentFrameColor.getSize().height, (u32)8);
                    buffer.dispatch(groupX, groupY, 1);
                }
        );
        return temporalAccumulationPass;
    };

    struct LightingMerge {
        Carrot::Render::PassData::GBuffer gBuffer;
        Render::FrameResource combinedLighting;
        Render::FrameResource ambientOcclusion;
        Render::FrameResource visibilityBufferDebug[DEBUG_VISIBILITY_BUFFER_LAST - DEBUG_VISIBILITY_BUFFER_FIRST + 1];
        Render::FrameResource mergeResult;
    };

    auto& mergeLighting = mainGraph.addPass<LightingMerge>(
            "merge-lighting",

            [this, lightingData, visibilityPasses, framebufferSize](Render::GraphBuilder& builder, Render::Pass<LightingMerge>& pass, LightingMerge& data) {
                data.gBuffer.readFrom(builder, lightingData.gBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.combinedLighting = builder.read(lightingData.combinedLighting, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.ambientOcclusion = builder.read(lightingData.ambientOcclusion, vk::ImageLayout::eShaderReadOnlyOptimal);

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
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], buffer, "merge-lighting");
                auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("post-process/merge-lighting", pass);

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
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.combinedLighting, frame.frameNumber), 1, 0, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.ambientOcclusion, frame.frameNumber), 1, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);

                for(int i = DEBUG_VISIBILITY_BUFFER_FIRST; i <= DEBUG_VISIBILITY_BUFFER_LAST; i++) {
                    int debugIndex = i - DEBUG_VISIBILITY_BUFFER_FIRST;
                    renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.visibilityBufferDebug[debugIndex], frame.frameNumber), 1, 2, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, debugIndex, vk::ImageLayout::eShaderReadOnlyOptimal);
                }

                pipeline->bind(pass, frame, buffer);
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
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], buffer, "draw-unlit");
                GetRenderer().recordPassPackets(Render::PassEnum::Unlit, pass, frame, buffer);
            });

    auto& toneMapping = mainGraph.addPass<Carrot::Render::PassData::PostProcessing>(
            "tone-mapping",

            [this, drawUnlit, framebufferSize](Render::GraphBuilder& builder, Render::Pass<Carrot::Render::PassData::PostProcessing>& pass, Carrot::Render::PassData::PostProcessing& data) {
                data.postLighting = builder.read(drawUnlit.getData().inout, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.postProcessed = builder.createRenderTarget("Tone mapped",
                                                                vk::Format::eR32G32B32A32Sfloat, // TODO: not sure why RGBA8_UNORM made the temporal blend fail in TAA
                                                                framebufferSize,
                                                                vk::AttachmentLoadOp::eClear,
                                                                vk::ClearColorValue(std::array{0,0,0,0}),
                                                                vk::ImageLayout::eColorAttachmentOptimal
                );
            },
            [this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::PostProcessing& data, vk::CommandBuffer& buffer) {
                ZoneScopedN("CPU RenderGraph post-process");
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], buffer, "Post-Process");
                auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("post-process/tone-mapping", pass);
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.postLighting, frame.frameNumber), 0, 0, nullptr);

                renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getNearestSampler(), 0, 1);
                renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getLinearSampler(), 0, 2);

                const GraphicsSettings::ToneMappingOption& toneMappingOption = renderer.getEngine().getSettings().graphicsSettings.toneMapping;
                renderer.pushConstants("entryPointParams", *pipeline, frame, vk::ShaderStageFlagBits::eFragment, buffer, toneMappingOption);
                pipeline->bind(pass, frame, buffer);
                auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                screenQuadMesh.bind(buffer);
                screenQuadMesh.draw(buffer);
            }
    );


    auto& finalTAA = makeTAAPass("final", toneMapping.getData().postProcessed, mergeLighting.getData().gBuffer,  mergeLighting.getData().gBuffer.positions, framebufferSize);

    return finalTAA.getData().denoisedResult;
}

const Carrot::Render::FrameResource& Carrot::Engine::fillGraphBuilder(Render::GraphBuilder& mainGraph, Render::Eye eye, const Render::TextureSize& framebufferSize) {
    return fillInDefaultPipeline(mainGraph, eye,
                                            [&](const Render::CompiledPass& pass, const Render::Context& frame, vk::CommandBuffer& cmds) {
                                                GPUZone(tracyCtx[frame.frameIndex], cmds, "Opaque Rendering");
                                                ZoneScopedN("CPU RenderGraph Opaque GPass");
                                                renderer.recordOpaqueGBufferPass(pass, frame, cmds);
                                            },
                                            [&](const Render::CompiledPass& pass, const Render::Context& frame, vk::CommandBuffer& cmds) {
                                                GPUZone(tracyCtx[frame.frameIndex], cmds, "Transparent Rendering");
                                                ZoneScopedN("CPU RenderGraph Transparent GPass");
                                                renderer.recordTransparentGBufferPass(pass, frame, cmds);
                                            },
                                            framebufferSize);

}