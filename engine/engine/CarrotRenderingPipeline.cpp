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

                    builder.reuseResourceAcrossFrames(data.denoisedResult, 1); // used in temporal algorithms

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
                    GetEngine().getResourceRepository().getTextureUsages(data.momentsHistoryHistoryLength.rootID) |= vk::ImageUsageFlagBits::eSampled;
                    GetEngine().getResourceRepository().getTextureUsages(data.firstSpatialDenoiseColor.rootID) |= vk::ImageUsageFlagBits::eSampled;
                },
                [this](const Render::CompiledPass& pass, const Render::Context& frame, const TemporalAccumulation& data, vk::CommandBuffer& buffer) {
                    ZoneScopedN("CPU RenderGraph temporal-denoise");
                    GPUZone(GetEngine().tracyCtx[frame.frameIndex], buffer, "temporal-denoise");
                    auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("post-process/temporal-denoise", pass);
                    renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.beauty, frame.frameNumber), 0, 0, nullptr);

                    auto bindLastFrameTexture = [&](const Render::FrameResource& resource, std::uint32_t bindingIndex, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal) {
                        Render::Texture& lastFrameTexture = pass.getGraph().getTexture(resource, frame.getPreviousFrameNumber());
                        renderer.bindTexture(*pipeline, frame, lastFrameTexture, 0, bindingIndex, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, layout);
                    };
                    bindLastFrameTexture(data.denoisedResult, 1, vk::ImageLayout::eGeneral);

                    Render::Texture& viewPosTexture = pass.getGraph().getTexture(data.viewPositions, frame.frameNumber);
                    renderer.bindTexture(*pipeline, frame, viewPosTexture, 0, 2, nullptr);
                    bindLastFrameTexture(data.viewPositions, 3);

                    bindLastFrameTexture(data.momentsHistoryHistoryLength, 5, vk::ImageLayout::eGeneral);

                    renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getNearestSampler(), 0, 6);
                    renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getLinearSampler(), 0, 7);

                    data.gBufferInput.bindInputs(*pipeline, frame, pass.getGraph(), 2, vk::ImageLayout::eShaderReadOnlyOptimal);

                    pipeline->bind(pass, frame, buffer);
                    auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                    screenQuadMesh.bind(buffer);
                    screenQuadMesh.draw(buffer);
                }
        );
        return temporalAccumulationPass;
    };

    struct LightingMerge {
        Carrot::Render::PassData::GBuffer gBuffer;
        Render::FrameResource directLighting;
        Render::FrameResource ambientOcclusion;
        Render::FrameResource reflections;
        Render::FrameResource gi;
        Render::FrameResource visibilityBufferDebug[DEBUG_VISIBILITY_BUFFER_LAST - DEBUG_VISIBILITY_BUFFER_FIRST + 1];
        Render::FrameResource mergeResult;
    };

    auto& mergeLighting = mainGraph.addPass<LightingMerge>(
            "merge-lighting",

            [this, lightingData, visibilityPasses, framebufferSize](Render::GraphBuilder& builder, Render::Pass<LightingMerge>& pass, LightingMerge& data) {
                data.gBuffer.readFrom(builder, lightingData.gBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.directLighting = builder.read(lightingData.direct, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.ambientOcclusion = builder.read(lightingData.ambientOcclusion, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.reflections = builder.read(lightingData.reflections, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.gi = builder.read(lightingData.gi, vk::ImageLayout::eShaderReadOnlyOptimal);

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
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.directLighting, frame.frameNumber), 1, 0, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.ambientOcclusion, frame.frameNumber), 1, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.reflections, frame.frameNumber), 1, 2, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.gi, frame.frameNumber), 1, 3, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);

                for(int i = DEBUG_VISIBILITY_BUFFER_FIRST; i <= DEBUG_VISIBILITY_BUFFER_LAST; i++) {
                    int debugIndex = i - DEBUG_VISIBILITY_BUFFER_FIRST;
                    renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.visibilityBufferDebug[debugIndex], frame.frameNumber), 1, 4, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, debugIndex, vk::ImageLayout::eShaderReadOnlyOptimal);
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
                                                                vk::Format::eR8G8B8A8Snorm,
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