//
// Created by jglrxavpok on 28/12/2020.
//

#include "GBuffer.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/render/Skybox.hpp"

Carrot::GBuffer::GBuffer(Carrot::VulkanRenderer& renderer, Carrot::RayTracer& raytracer): renderer(renderer), raytracer(raytracer) {

}

void Carrot::GBuffer::onSwapchainImageCountChange(size_t newCount) {

}

void Carrot::GBuffer::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {
    // TODO
}

Carrot::Render::Pass<Carrot::Render::PassData::GBuffer>& Carrot::GBuffer::addGBufferPass(Carrot::Render::GraphBuilder& graph, std::function<void(const Carrot::Render::CompiledPass& pass, const Carrot::Render::Context&, vk::CommandBuffer&)> opaqueCallback, const Render::TextureSize& framebufferSize) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
    vk::ClearValue positionClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue{
            .depth = 1.0f,
            .stencil = 0
    };
    vk::ClearValue clearIntProperties = vk::ClearColorValue();
    vk::ClearValue clearEntityID = vk::ClearColorValue(std::array<std::uint32_t,4>{0,0,0,0});
    auto& opaquePass = graph.addPass<Carrot::Render::PassData::GBuffer>("gbuffer",
           [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::GBuffer>& pass, Carrot::Render::PassData::GBuffer& data)
           {

                data.albedo = graph.createRenderTarget("Albedo",
                                                       vk::Format::eR8G8B8A8Unorm,
                                                       framebufferSize,
                                                       vk::AttachmentLoadOp::eClear,
                                                       clearColor,
                                                       vk::ImageLayout::eColorAttachmentOptimal);

                data.positions = graph.createRenderTarget("View Positions",
                                                          vk::Format::eR32G32B32A32Sfloat,
                                                          framebufferSize,
                                                          vk::AttachmentLoadOp::eClear,
                                                          positionClear,
                                                          vk::ImageLayout::eColorAttachmentOptimal);

                data.viewSpaceNormalTangents = graph.createRenderTarget("View space normals tangents",
                                                        vk::Format::eR32G32B32A32Sfloat,
                                                        framebufferSize,
                                                        vk::AttachmentLoadOp::eClear,
                                                        positionClear,
                                                        vk::ImageLayout::eColorAttachmentOptimal);

                data.flags = graph.createRenderTarget("Flags",
                                                      vk::Format::eR32Uint,
                                                      framebufferSize,
                                                      vk::AttachmentLoadOp::eClear,
                                                      clearIntProperties,
                                                      vk::ImageLayout::eColorAttachmentOptimal);

                data.entityID = graph.createRenderTarget("EntityID",
                                                         vk::Format::eR32G32B32A32Uint,
                                                        framebufferSize,
                                                        vk::AttachmentLoadOp::eClear,
                                                        clearEntityID,
                                                        vk::ImageLayout::eColorAttachmentOptimal);

               data.metallicRoughnessVelocityXY = graph.createRenderTarget("MetallicnessRoughness+VelocityXY",
                                                                           vk::Format::eR32G32B32A32Sfloat,
                                                                           framebufferSize,
                                                                           vk::AttachmentLoadOp::eClear,
                                                                           clearColor,
                                                                           vk::ImageLayout::eColorAttachmentOptimal);

               data.emissiveVelocityZ = graph.createRenderTarget("Emissive + Velocity Z",
                                                                 vk::Format::eR32G32B32A32Sfloat,
                                                                 framebufferSize,
                                                                 vk::AttachmentLoadOp::eClear,
                                                                 clearColor,
                                                                 vk::ImageLayout::eColorAttachmentOptimal);

               data.depthStencil = graph.createRenderTarget("Depth Stencil",
                                                            renderer.getVulkanDriver().getDepthFormat(),
                                                            framebufferSize,
                                                            vk::AttachmentLoadOp::eClear,
                                                            clearDepth,
                                                            vk::ImageLayout::eDepthStencilAttachmentOptimal);
           },
           [opaqueCallback](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::GBuffer& data, vk::CommandBuffer& buffer){
                opaqueCallback(pass, frame, buffer);
           }
    );
    return opaquePass;
}

Carrot::Render::Pass<Carrot::Render::PassData::Lighting>& Carrot::GBuffer::addLightingPass(const Carrot::Render::PassData::GBuffer& opaqueData,
                                                                                           Carrot::Render::GraphBuilder& graph,
                                                                                           const Render::TextureSize& framebufferSize) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});

    return graph.addPass<Carrot::Render::PassData::Lighting>("lighting",
                                                             [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::Lighting>& pass, Carrot::Render::PassData::Lighting& resolveData)
           {
                pass.rasterized = false;
                pass.prerecordable = false; //TODO: since it depends on Lighting descriptor sets which may change, it is not 100% pre-recordable now
                // TODO (or it should be re-recorded when changes happen)
                resolveData.gBuffer.readFrom(graph, opaqueData, vk::ImageLayout::eShaderReadOnlyOptimal);

                resolveData.directLighting = graph.createStorageTarget("Direct Lighting",
                                                                vk::Format::eR32G32B32A32Sfloat,
                                                                framebufferSize,
                                                                vk::ImageLayout::eGeneral);
                resolveData.ambientOcclusion = graph.createStorageTarget("Ambient Occlusion",
                                                                vk::Format::eR8Unorm,
                                                                framebufferSize,
                                                                vk::ImageLayout::eGeneral);
           },
           [framebufferSize, this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::Lighting& data, vk::CommandBuffer& buffer) {
                ZoneScopedN("CPU RenderGraph lighting");
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], buffer, "lighting");
                bool useRaytracingVersion = GetCapabilities().supportsRaytracing;

                const char* directLightingShader = useRaytracingVersion ? "lighting/direct-raytracing" : "lighting/direct-noraytracing";
                auto directLightingPipeline = renderer.getOrCreatePipeline(directLightingShader,  (std::uint64_t)&pass);

                const char* aoShader = useRaytracingVersion ? "lighting/ao-raytracing" : "lighting/ao-noraytracing";
                auto aoPipeline = renderer.getOrCreatePipeline(aoShader,  (std::uint64_t)&pass);

                // used for randomness
                struct PushConstantNoRT {
                    std::uint32_t frameCount;
                    std::uint32_t frameWidth;
                    std::uint32_t frameHeight;
                };
                struct PushConstantRT: PushConstantNoRT {
                    bool hasTLAS = false; // used only if RT is supported by GPU. Shader compilation will strip this member in shaders where it is unused!
                } block;

                block.frameCount = renderer.getFrameCount();
                if(framebufferSize.type == Render::TextureSize::Type::SwapchainProportional) {
                    block.frameWidth = framebufferSize.width * frame.pViewport->getWidth();
                    block.frameHeight = framebufferSize.height * frame.pViewport->getHeight();
                } else {
                    block.frameWidth = framebufferSize.width;
                    block.frameHeight = framebufferSize.height;
                }

                auto setupPipeline = [&](const FrameResource& output, Carrot::Pipeline& pipeline, bool needSceneInfo) {
                    Carrot::AccelerationStructure* pTLAS = nullptr;
                    if(useRaytracingVersion) {
                        pTLAS = frame.renderer.getASBuilder().getTopLevelAS(frame);
                        block.hasTLAS = pTLAS != nullptr;
                        renderer.pushConstantBlock<PushConstantRT>("push", pipeline, frame, vk::ShaderStageFlagBits::eCompute, buffer, block);
                    } else {
                        renderer.pushConstantBlock<PushConstantNoRT>("push", pipeline, frame, vk::ShaderStageFlagBits::eCompute, buffer, block);
                    }

                    // GBuffer inputs
                    data.gBuffer.bindInputs(pipeline, frame, pass.getGraph(), 0, vk::ImageLayout::eShaderReadOnlyOptimal);

                    // outputs
                    auto& renderGraph = pass.getGraph();
                    auto& outputImage = renderGraph.getTexture(output, frame.swapchainIndex);
                    renderer.bindStorageImage(pipeline, frame, outputImage, 5, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                    if(useRaytracingVersion) {
                        renderer.bindTexture(pipeline, frame, *renderer.getMaterialSystem().getBlueNoiseTextures()[frame.swapchainIndex % Render::BlueNoiseTextureCount]->texture, 6, 1, nullptr);
                        if(pTLAS) {
                            renderer.bindAccelerationStructure(pipeline, frame, *pTLAS, 6, 0);
                            if(needSceneInfo) {
                                renderer.bindBuffer(pipeline, frame, renderer.getASBuilder().getGeometriesBuffer(frame), 6, 2);
                                renderer.bindBuffer(pipeline, frame, renderer.getASBuilder().getInstancesBuffer(frame), 6, 3);
                            }
                        } else {
                            renderer.unbindAccelerationStructure(pipeline, frame, 6, 0);
                            if(needSceneInfo) {
                                renderer.unbindBuffer(pipeline, frame, 6, 2);
                                renderer.unbindBuffer(pipeline, frame, 6, 3);
                            }
                        }
                    }
                };

                const std::uint8_t localSizeX = 32;
                const std::uint8_t localSizeY = 32;
                std::size_t dispatchX = (block.frameWidth + (localSizeX-1)) / localSizeX;
                std::size_t dispatchY = (block.frameHeight + (localSizeY-1)) / localSizeY;

                setupPipeline(data.directLighting, *directLightingPipeline, false);
                directLightingPipeline->bind({}, frame, buffer, vk::PipelineBindPoint::eCompute);
                buffer.dispatch(dispatchX, dispatchY, 1);

                setupPipeline(data.ambientOcclusion, *aoPipeline, false);
                aoPipeline->bind({}, frame, buffer, vk::PipelineBindPoint::eCompute);
                buffer.dispatch(dispatchX, dispatchY, 1);

                vk::MemoryBarrier2KHR memoryBarrier {
                        .srcStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                        .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                        .dstStageMask = vk::PipelineStageFlagBits2KHR::eAllCommands,
                        .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead,
                };
                vk::DependencyInfoKHR dependencyInfo {
                        .memoryBarrierCount = 1,
                        .pMemoryBarriers = &memoryBarrier,
                };
                buffer.pipelineBarrier2KHR(dependencyInfo);
           }
    );
}

void Carrot::Render::PassData::GBuffer::readFrom(Render::GraphBuilder& graph, const GBuffer& other, vk::ImageLayout wantedLayout) {
    albedo = graph.read(other.albedo, wantedLayout);
    positions = graph.read(other.positions, wantedLayout);
    viewSpaceNormalTangents = graph.read(other.viewSpaceNormalTangents, wantedLayout);
    flags = graph.read(other.flags, wantedLayout);
    entityID = graph.read(other.entityID, wantedLayout);
    metallicRoughnessVelocityXY = graph.read(other.metallicRoughnessVelocityXY, wantedLayout);
    emissiveVelocityZ = graph.read(other.emissiveVelocityZ, wantedLayout);
    depthStencil = graph.read(other.depthStencil, vk::ImageLayout::eDepthStencilReadOnlyOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);

    // TODO: fix (double read in two != passes result in no 'previousLayout' change
    depthStencil.previousLayout = depthStencil.layout;
    albedo.previousLayout = albedo.layout;
}

void Carrot::Render::PassData::GBuffer::writeTo(Render::GraphBuilder& graph, const GBuffer& other) {
    albedo = graph.write(other.albedo, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    positions = graph.write(other.positions, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    viewSpaceNormalTangents = graph.write(other.viewSpaceNormalTangents, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    flags = graph.write(other.flags, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    entityID = graph.write(other.entityID, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    metallicRoughnessVelocityXY = graph.write(other.metallicRoughnessVelocityXY, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    emissiveVelocityZ = graph.write(other.emissiveVelocityZ, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    depthStencil = graph.write(other.depthStencil, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    // TODO: fix (double read in two != passes result in no 'previousLayout' change
    depthStencil.previousLayout = depthStencil.layout;
    albedo.previousLayout = albedo.layout;
}

void Carrot::Render::PassData::GBuffer::bindInputs(Carrot::Pipeline& pipeline, const Render::Context& frame, const Render::Graph& renderGraph, std::uint32_t setID, vk::ImageLayout expectedLayout) const {
    auto& renderer = GetRenderer();
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(albedo, frame.swapchainIndex), setID, 0, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(positions, frame.swapchainIndex), setID, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(viewSpaceNormalTangents, frame.swapchainIndex), setID, 2, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(flags, frame.swapchainIndex), setID, 3, renderer.getVulkanDriver().getNearestSampler(), vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(entityID, frame.swapchainIndex), setID, 4, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(metallicRoughnessVelocityXY, frame.swapchainIndex), setID, 5, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(emissiveVelocityZ, frame.swapchainIndex), setID, 6, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);

    vk::ImageLayout depthLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
    if(expectedLayout == vk::ImageLayout::eGeneral) {
        depthLayout = vk::ImageLayout::eGeneral;
    } else if(expectedLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        depthLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    }
    // 7 unused
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(depthStencil, frame.swapchainIndex), setID, 8, nullptr, vk::ImageAspectFlagBits::eDepth, vk::ImageViewType::e2D, 0, depthLayout);

    Render::Texture::Ref skyboxCubeMap = GetEngine().getSkyboxCubeMap();
    if(!skyboxCubeMap || GetEngine().getSkybox() == Carrot::Skybox::Type::None) {
        skyboxCubeMap = renderer.getBlackCubeMapTexture();
    }
    renderer.bindTexture(pipeline, frame, *skyboxCubeMap, setID, 9, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::eCube);

    renderer.bindSampler(pipeline, frame, renderer.getVulkanDriver().getLinearSampler(), setID, 10);
    renderer.bindSampler(pipeline, frame, renderer.getVulkanDriver().getNearestSampler(), setID, 11);
}
