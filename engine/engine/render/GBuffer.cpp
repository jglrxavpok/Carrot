//
// Created by jglrxavpok on 28/12/2020.
//

#include "GBuffer.h"

#include <core/io/Logging.hpp>
#include <engine/Engine.h>

#include "engine/render/raytracing/RaytracingScene.h"
#include "engine/render/Skybox.hpp"
#include "resources/ResourceAllocator.h"
#include <engine/render/VulkanRenderer.h>
#include <engine/vulkan/VulkanDriver.h>

Carrot::GBuffer::GBuffer(Carrot::VulkanRenderer& renderer, Carrot::RayTracer& raytracer): renderer(renderer), raytracer(raytracer) {

}

void Carrot::GBuffer::onSwapchainImageCountChange(size_t newCount) {

}

void Carrot::GBuffer::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {
    // TODO
}

Carrot::Render::Pass<Carrot::Render::PassData::GBuffer>& Carrot::GBuffer::addGBufferPass(Carrot::Render::GraphBuilder& graph, std::function<void(const Carrot::Render::CompiledPass& pass, const Carrot::Render::Context&, vk::CommandBuffer&)> opaqueCallback, const Render::TextureSize& framebufferSize,
    std::optional<Render::FrameResource> inheritedDepthStencil) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
    vk::ClearValue positionClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue{
            .depth = 1.0f,
            .stencil = 0
    };
    vk::ClearValue clearIntProperties = vk::ClearColorValue();
    vk::ClearValue clearEntityID = vk::ClearColorValue(std::array<std::uint32_t,4>{0,0,0,0});
    auto& setupGBuffer = graph.addPass<Carrot::Render::PassData::GBuffer>("setup-gbuffer",
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
                                                        vk::Format::eR16G16B16A16Sfloat,
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

               data.emissive = graph.createRenderTarget("Emissive",
                                                                 vk::Format::eR32G32B32A32Sfloat /*RGB32Sfloat not supported by RADV*/,
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

               if (inheritedDepthStencil.has_value()) {
                   data.inheritedStencil = graph.read(inheritedDepthStencil.value(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
               }

               // used by editor & temporal algorithms
               graph.reuseResourceAcrossFrames(data.albedo, 1);
               graph.reuseResourceAcrossFrames(data.positions, 1);
               graph.reuseResourceAcrossFrames(data.viewSpaceNormalTangents, 1);
               graph.reuseResourceAcrossFrames(data.entityID, 1);
               graph.reuseResourceAcrossFrames(data.metallicRoughnessVelocityXY, 1);
               graph.reuseResourceAcrossFrames(data.emissive, 1);
               graph.reuseResourceAcrossFrames(data.depthStencil, 1);
           },
           [](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::GBuffer& data, vk::CommandBuffer& cmds){
               ZoneScopedN("CPU RenderGraph setup-gbuffer");
               GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "setup-gbuffer");

           }
    );

    // need separate pass because copies are not allowed during rendering
    auto& copyStencil = graph.addPass<PassData::GBuffer>("copy stencil",
        [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::GBuffer>& pass, Carrot::Render::PassData::GBuffer& data) {
            data.writeTo(graph, setupGBuffer.getData(), vk::ImageLayout::eColorAttachmentOptimal);
            data.inheritedStencil = setupGBuffer.getData().inheritedStencil;

            pass.rasterized = false;
        },
        [opaqueCallback](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::GBuffer& data, vk::CommandBuffer& cmds) {
            if (data.inheritedStencil.has_value()) {
               vk::ImageCopy stencilCopy {
                   .srcSubresource = {
                       .aspectMask = vk::ImageAspectFlagBits::eStencil,
                       .layerCount = 1,
                   },
                   .dstSubresource = {
                       .aspectMask = vk::ImageAspectFlagBits::eStencil,
                       .layerCount = 1,
                   },
                   .extent = {
                       .width = frame.pViewport->getWidth(),
                       .height = frame.pViewport->getHeight(),
                       .depth = 1,
                   },
               };

               const FrameResource& stencilToCopy = data.inheritedStencil.value();
               auto& source = pass.getGraph().getTexture(stencilToCopy, frame.frameNumber);
               auto& destination = pass.getGraph().getTexture(data.depthStencil, frame.frameNumber);

               // TODO: remove this transition
               destination.assumeLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
               destination.transitionInline(cmds, vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
               cmds.copyImage(source.getVulkanImage(), vk::ImageLayout::eTransferSrcOptimal, destination.getVulkanImage(), vk::ImageLayout::eTransferDstOptimal, stencilCopy);
               destination.assumeLayout(vk::ImageLayout::eTransferDstOptimal);
               destination.transitionInline(cmds, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
           }
        }
    );

    auto& opaquePass = graph.addPass<PassData::GBuffer>("gbuffer",
        [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::GBuffer>& pass, Carrot::Render::PassData::GBuffer& data) {
            data.writeTo(graph, copyStencil.getData(), vk::ImageLayout::eColorAttachmentOptimal);
        },
        [opaqueCallback](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::GBuffer& data, vk::CommandBuffer& cmds) {
            ZoneScopedN("CPU RenderGraph gbuffer");
            GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "gbuffer");
            opaqueCallback(pass, frame, cmds);
        }
    );
    return opaquePass;
}

void Carrot::Render::PassData::GBuffer::readFrom(Render::GraphBuilder& graph, const GBuffer& other, vk::ImageLayout wantedLayout) {
    albedo = graph.read(other.albedo, wantedLayout);
    positions = graph.read(other.positions, wantedLayout);
    viewSpaceNormalTangents = graph.read(other.viewSpaceNormalTangents, wantedLayout);
    flags = graph.read(other.flags, wantedLayout);
    entityID = graph.read(other.entityID, wantedLayout);
    metallicRoughnessVelocityXY = graph.read(other.metallicRoughnessVelocityXY, wantedLayout);
    emissive = graph.read(other.emissive, wantedLayout);

    vk::ImageLayout depthLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
    if (wantedLayout == vk::ImageLayout::eGeneral) {
        depthLayout = vk::ImageLayout::eGeneral;
    }
    depthStencil = graph.read(other.depthStencil, depthLayout, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);

    // TODO: fix (double read in two != passes result in no 'previousLayout' change
    depthStencil.previousLayout = depthStencil.layout;
    albedo.previousLayout = albedo.layout;
}

void Carrot::Render::PassData::GBuffer::writeTo(Render::GraphBuilder& graph, const GBuffer& other, vk::ImageLayout wantedLayout) {
    albedo = graph.write(other.albedo, vk::AttachmentLoadOp::eLoad, wantedLayout);
    positions = graph.write(other.positions, vk::AttachmentLoadOp::eLoad, wantedLayout);
    viewSpaceNormalTangents = graph.write(other.viewSpaceNormalTangents, vk::AttachmentLoadOp::eLoad, wantedLayout);
    flags = graph.write(other.flags, vk::AttachmentLoadOp::eLoad, wantedLayout);
    entityID = graph.write(other.entityID, vk::AttachmentLoadOp::eLoad, wantedLayout);
    metallicRoughnessVelocityXY = graph.write(other.metallicRoughnessVelocityXY, vk::AttachmentLoadOp::eLoad, wantedLayout);
    emissive = graph.write(other.emissive, vk::AttachmentLoadOp::eLoad, wantedLayout);

    vk::ImageLayout depthLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    if (wantedLayout == vk::ImageLayout::eGeneral) {
        depthLayout = vk::ImageLayout::eGeneral;
    }
    depthStencil = graph.write(other.depthStencil, vk::AttachmentLoadOp::eLoad, depthLayout, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);

    // TODO: fix (double read in two != passes result in no 'previousLayout' change
    depthStencil.previousLayout = depthStencil.layout;
    albedo.previousLayout = albedo.layout;
}

void Carrot::Render::PassData::GBuffer::bindLastFrameInputs(Carrot::Pipeline& pipeline, const Render::Context& frame, const Render::Graph& renderGraph, std::uint32_t setID, vk::ImageLayout expectedLayout) const {
    bindInputsFromGivenFrame(pipeline, frame, frame.getPreviousFrameNumber(), renderGraph, setID, expectedLayout);
}

void Carrot::Render::PassData::GBuffer::bindInputs(Carrot::Pipeline& pipeline, const Render::Context& frame, const Render::Graph& renderGraph, std::uint32_t setID, vk::ImageLayout expectedLayout) const {
    bindInputsFromGivenFrame(pipeline, frame, frame.frameNumber, renderGraph, setID, expectedLayout);
}

void Carrot::Render::PassData::GBuffer::bindInputsFromGivenFrame(Carrot::Pipeline& pipeline, const Render::Context& frame, u64 frameNumber, const Render::Graph& renderGraph, std::uint32_t setID, vk::ImageLayout expectedLayout) const {
    auto& renderer = GetRenderer();
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(albedo, frameNumber), setID, 0, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(positions, frameNumber), setID, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(viewSpaceNormalTangents, frameNumber), setID, 2, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(flags, frameNumber), setID, 3, renderer.getVulkanDriver().getNearestSampler(), vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(entityID, frameNumber), setID, 4, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(metallicRoughnessVelocityXY, frameNumber), setID, 5, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(emissive, frameNumber), setID, 6, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);

    vk::ImageLayout depthLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
    if(expectedLayout == vk::ImageLayout::eGeneral) {
        depthLayout = vk::ImageLayout::eGeneral;
    } else if(expectedLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        depthLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    }
    // 7 unused
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(depthStencil, frameNumber), setID, 8, nullptr, vk::ImageAspectFlagBits::eDepth, vk::ImageViewType::e2D, 0, depthLayout);

    Render::Texture::Ref skyboxCubeMap = GetEngine().getSkyboxCubeMap();
    if(!skyboxCubeMap || GetEngine().getSkybox() == Carrot::Skybox::Type::None) {
        skyboxCubeMap = renderer.getBlackCubeMapTexture();
    }
    renderer.bindTexture(pipeline, frame, *skyboxCubeMap, setID, 9, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::eCube);

    renderer.bindSampler(pipeline, frame, renderer.getVulkanDriver().getLinearSampler(), setID, 10);
    renderer.bindSampler(pipeline, frame, renderer.getVulkanDriver().getNearestSampler(), setID, 11);
}

void Carrot::Render::PassData::GBuffer::bindRW(Carrot::Pipeline& pipeline, const Render::Context& frame, const Render::Graph& renderGraph, std::uint32_t setID) const {
    const u64 frameNumber = frame.frameNumber;
    const vk::ImageLayout expectedLayout = vk::ImageLayout::eGeneral;
    auto& renderer = GetRenderer();
    renderer.bindStorageImage(pipeline, frame, renderGraph.getTexture(albedo, frameNumber), setID, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindStorageImage(pipeline, frame, renderGraph.getTexture(positions, frameNumber), setID, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindStorageImage(pipeline, frame, renderGraph.getTexture(viewSpaceNormalTangents, frameNumber), setID, 2, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindStorageImage(pipeline, frame, renderGraph.getTexture(flags, frameNumber), setID, 3, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindStorageImage(pipeline, frame, renderGraph.getTexture(entityID, frameNumber), setID, 4, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindStorageImage(pipeline, frame, renderGraph.getTexture(metallicRoughnessVelocityXY, frameNumber), setID, 5, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindStorageImage(pipeline, frame, renderGraph.getTexture(emissive, frameNumber), setID, 6, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);

    vk::ImageLayout depthLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
    if(expectedLayout == vk::ImageLayout::eGeneral) {
        depthLayout = vk::ImageLayout::eGeneral;
    } else if(expectedLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        depthLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    }
    // 7 unused
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(depthStencil, frameNumber), setID, 8, nullptr, vk::ImageAspectFlagBits::eDepth, vk::ImageViewType::e2D, 0, depthLayout);

    Render::Texture::Ref skyboxCubeMap = GetEngine().getSkyboxCubeMap();
    if(!skyboxCubeMap || GetEngine().getSkybox() == Carrot::Skybox::Type::None) {
        skyboxCubeMap = renderer.getBlackCubeMapTexture();
    }
    renderer.bindTexture(pipeline, frame, *skyboxCubeMap, setID, 9, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::eCube);

    renderer.bindSampler(pipeline, frame, renderer.getVulkanDriver().getLinearSampler(), setID, 10);
    renderer.bindSampler(pipeline, frame, renderer.getVulkanDriver().getNearestSampler(), setID, 11);
}
