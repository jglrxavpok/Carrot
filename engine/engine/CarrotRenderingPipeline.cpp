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
    /* TODO: remove*/
    auto& skyboxPass = mainGraph.addPass<Carrot::Render::PassData::Skybox>("skybox",
                                                                           [this, framebufferSize](Render::GraphBuilder& builder, Render::Pass<Carrot::Render::PassData::Skybox>& pass, Carrot::Render::PassData::Skybox& data) {
                                                                               data.output = builder.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                                                                                        framebufferSize,
                                                                                                                        vk::AttachmentLoadOp::eClear,
                                                                                                                        vk::ClearColorValue(std::array{0,0,0,0})
                                                                               );
                                                                           },
                                                                           [this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::Skybox& data, vk::CommandBuffer& buffer) {
                                                                               ZoneScopedN("CPU RenderGraph skybox");
                                                                               auto skyboxPipeline = renderer.getOrCreateRenderPassSpecificPipeline("skybox", pass.getRenderPass());
                                                                               renderer.bindTexture(*skyboxPipeline, frame, *loadedSkyboxTexture, 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::eCube);
                                                                               skyboxPipeline->bind(pass.getRenderPass(), frame, buffer);
                                                                               skyboxMesh->bind(buffer);
                                                                               skyboxMesh->draw(buffer);
                                                                           }
    );
    skyboxPass.setCondition([this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::Skybox& data) {
        return false;//currentSkybox != Skybox::Type::None;
    });

    auto& opaqueGBufferPass = getGBuffer().addGBufferPass(mainGraph, [opaqueCallback](const Render::CompiledPass& pass, const Render::Context& frame, vk::CommandBuffer& cmds) {
        ZoneScopedN("CPU RenderGraph Opaque GPass");
        opaqueCallback(pass, frame, cmds);
    }, framebufferSize);
    auto& transparentGBufferPass = getGBuffer().addTransparentGBufferPass(mainGraph, opaqueGBufferPass.getData(), [transparentCallback](const Render::CompiledPass& pass, const Render::Context& frame, vk::CommandBuffer& cmds) {
        ZoneScopedN("CPU RenderGraph Opaque GPass");
        transparentCallback(pass, frame, cmds);
    }, framebufferSize);
    auto& lightingPass = getGBuffer().addLightingPass(opaqueGBufferPass.getData(), transparentGBufferPass.getData(), skyboxPass.getData().output, mainGraph, framebufferSize);

    struct Denoising {
        Render::FrameResource beauty;
        Render::FrameResource viewSpacePositions;
        Render::FrameResource motionVectors;
        Render::FrameResource momentsHistoryHistoryLength; // vec4(moment, moment², history length, __unused__)
        Render::FrameResource denoisedResult;
    };
    auto& denoisingPass = mainGraph.addPass<Denoising>(
            "temporal-denoise",
            [this, lightingPass, framebufferSize](Render::GraphBuilder& builder, Render::Pass<Denoising>& pass, Denoising& data) {
                data.beauty = builder.read(lightingPass.getData().resolved, vk::ImageLayout::eShaderReadOnlyOptimal);
                // TODO: use entire GBuffer
                data.viewSpacePositions = builder.read(lightingPass.getData().gBuffer.positions, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.motionVectors = builder.read(lightingPass.getData().gBuffer.velocity, vk::ImageLayout::eShaderReadOnlyOptimal);
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

                Render::Texture& viewPosTexture = pass.getGraph().getTexture(data.viewSpacePositions, frame.swapchainIndex);
                renderer.bindTexture(*pipeline, frame, viewPosTexture, 0, 2, nullptr);
                bindLastFrameTexture(data.viewSpacePositions, 3);

                renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.motionVectors, frame.swapchainIndex), 0, 4, nullptr);
                bindLastFrameTexture(data.momentsHistoryHistoryLength, 5);

                renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getNearestSampler(), 0, 6);
                renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getLinearSampler(), 0, 7);

                renderer.bindUniformBuffer(*pipeline, frame, frame.viewport.getCameraUniformBuffer(frame), 1, 0);
                renderer.bindUniformBuffer(*pipeline, frame, frame.viewport.getCameraUniformBuffer(frame.lastFrame()), 1, 1);

                pipeline->bind(pass.getRenderPass(), frame, buffer);
                auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                screenQuadMesh.bind(buffer);
                screenQuadMesh.draw(buffer);
            }
    );

    // TODO: spatial filtering on temporally-denoised output

    struct LightingMerge {
        Carrot::Render::PassData::GBuffer gBuffer;
        Render::FrameResource lighting;
        Render::FrameResource momentsHistoryHistoryLength;
        Render::FrameResource mergeResult;
    };

    auto& mergeLighting = mainGraph.addPass<LightingMerge>(
            "merge-lighting",

            [this, lightingPass, denoisingPass, skyboxData = skyboxPass.getData(), framebufferSize](Render::GraphBuilder& builder, Render::Pass<LightingMerge>& pass, LightingMerge& data) {
                data.gBuffer.readFrom(builder, lightingPass.getData().gBuffer);
                data.lighting = builder.read(denoisingPass.getData().denoisedResult, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.momentsHistoryHistoryLength = builder.read(denoisingPass.getData().momentsHistoryHistoryLength, vk::ImageLayout::eShaderReadOnlyOptimal);
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

                pipeline->bind(pass.getRenderPass(), frame, buffer);
                auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                screenQuadMesh.bind(buffer);
                screenQuadMesh.draw(buffer);
            }
    );

    struct TestComputeData {
        Render::FrameResource postProcessed;
        Render::FrameResource mergedResult;
    };
    auto& testCompute = mainGraph.addPass<TestComputeData>(
            "testCompute",

            [this, mergeLighting, framebufferSize](Render::GraphBuilder& builder, Render::Pass<TestComputeData>& pass, TestComputeData& data) {
                data.postProcessed = builder.read(mergeLighting.getData().mergeResult, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.mergedResult = builder.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                               framebufferSize,
                                                               vk::AttachmentLoadOp::eClear,
                                                               vk::ClearColorValue(std::array{0,0,0,0}),
                                                               vk::ImageLayout::eColorAttachmentOptimal
                );

                pass.rasterized = false; // compute pass
            },
            [this](const Render::CompiledPass& pass, const Render::Context& frame, const TestComputeData& data, vk::CommandBuffer& buffer) {
                ZoneScopedN("CPU RenderGraph testCompute");
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], buffer, "testCompute");

                const auto& extent = GetVulkanDriver().getWindowFramebufferExtent();

                auto pipeline = frame.renderer.getOrCreatePipeline("compute/test-compute");
                pipeline->bind({}, frame, buffer, vk::PipelineBindPoint::eCompute);
                frame.renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.postProcessed, frame.swapchainIndex), 0, 0, nullptr);
                frame.renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.mergedResult, frame.swapchainIndex), 0, 1, nullptr);
                buffer.dispatch(extent.width, extent.height, 1);
            }
    );

    auto& toneMapping = mainGraph.addPass<Carrot::Render::PassData::PostProcessing>(
            "tone-mapping",

            [this, testCompute, framebufferSize](Render::GraphBuilder& builder, Render::Pass<Carrot::Render::PassData::PostProcessing>& pass, Carrot::Render::PassData::PostProcessing& data) {
                data.postLighting = builder.read(testCompute.getData().mergedResult, vk::ImageLayout::eShaderReadOnlyOptimal);
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