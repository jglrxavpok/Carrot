//
// Created by jglrxavpok on 28/12/2020.
//

#include "GBuffer.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/render/Skybox.hpp"

Carrot::GBuffer::GBuffer(Carrot::VulkanRenderer& renderer, Carrot::RayTracer& raytracer): renderer(renderer), raytracer(raytracer) {
    blueNoise = renderer.getOrCreateTexture("FreeBlueNoiseTextures/LDR_RGB1_54.png");
}

void Carrot::GBuffer::onSwapchainImageCountChange(size_t newCount) {

}

void Carrot::GBuffer::onSwapchainSizeChange(int newWidth, int newHeight) {
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

                data.albedo = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                       framebufferSize,
                                                       vk::AttachmentLoadOp::eClear,
                                                       clearColor,
                                                       vk::ImageLayout::eColorAttachmentOptimal);

                data.positions = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                          framebufferSize,
                                                          vk::AttachmentLoadOp::eClear,
                                                          positionClear,
                                                          vk::ImageLayout::eColorAttachmentOptimal);

                data.viewSpaceNormalTangents = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                        framebufferSize,
                                                        vk::AttachmentLoadOp::eClear,
                                                        positionClear,
                                                        vk::ImageLayout::eColorAttachmentOptimal);

                data.flags = graph.createRenderTarget(vk::Format::eR32Uint,
                                                      framebufferSize,
                                                      vk::AttachmentLoadOp::eClear,
                                                      clearIntProperties,
                                                      vk::ImageLayout::eColorAttachmentOptimal);

                data.entityID = graph.createRenderTarget(vk::Format::eR32G32B32A32Uint,
                                                        framebufferSize,
                                                        vk::AttachmentLoadOp::eClear,
                                                        clearEntityID,
                                                        vk::ImageLayout::eColorAttachmentOptimal);

               data.metallicRoughnessVelocityXY = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                                 framebufferSize,
                                                                 vk::AttachmentLoadOp::eClear,
                                                                 clearColor,
                                                                 vk::ImageLayout::eColorAttachmentOptimal);

               data.emissiveVelocityZ = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                        framebufferSize,
                                                        vk::AttachmentLoadOp::eClear,
                                                        clearColor,
                                                        vk::ImageLayout::eColorAttachmentOptimal);

               data.depthStencil = graph.createRenderTarget(renderer.getVulkanDriver().getDepthFormat(),
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

    const float scaleFactor = 0.75f;
    TextureSize outputSize;
    outputSize.type = framebufferSize.type;
    outputSize.width = scaleFactor * framebufferSize.width;
    outputSize.height = scaleFactor * framebufferSize.height;

    return graph.addPass<Carrot::Render::PassData::Lighting>("lighting",
                                                             [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::Lighting>& pass, Carrot::Render::PassData::Lighting& resolveData)
           {
                // pass.prerecordable = true; TODO: since it depends on Lighting descriptor sets which may change, it is not 100% pre-recordable now
                // TODO (or it should be re-recorded when changes happen)
                resolveData.gBuffer.readFrom(graph, opaqueData);

               // TODO: output into multiple buffer depending on content type (shadows, reflections)
                resolveData.resolved = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                                outputSize,
                                                                vk::AttachmentLoadOp::eClear,
                                                                clearColor,
                                                                vk::ImageLayout::eColorAttachmentOptimal);
           },
           [outputSize, this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::Lighting& data, vk::CommandBuffer& buffer) {
                ZoneScopedN("CPU RenderGraph lighting");
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], buffer, "lighting");
                bool useRaytracingVersion = GetCapabilities().supportsRaytracing;
                const char* shader = useRaytracingVersion ? "lighting-raytracing" : "lighting-noraytracing";
                auto resolvePipeline = renderer.getOrCreateRenderPassSpecificPipeline(shader, pass.getRenderPass());

                struct PushConstant {
                    std::uint32_t frameCount;
                    std::uint32_t frameWidth;
                    std::uint32_t frameHeight;

                    bool hasTLAS = false;
                } block;

                block.frameCount = renderer.getFrameCount();
                if(outputSize.type == Render::TextureSize::Type::SwapchainProportional) {
                    block.frameWidth = outputSize.width * GetVulkanDriver().getWindowFramebufferExtent().width;
                    block.frameHeight = outputSize.height * GetVulkanDriver().getWindowFramebufferExtent().height;
                } else {
                    block.frameWidth = outputSize.width;
                    block.frameHeight = outputSize.height;
                }
                if(useRaytracingVersion) {
                    block.hasTLAS = frame.renderer.getASBuilder().getTopLevelAS() != nullptr;
                }
                renderer.pushConstantBlock("push", *resolvePipeline, frame, vk::ShaderStageFlagBits::eFragment, buffer, block);

                // GBuffer inputs
                data.gBuffer.bindInputs(*resolvePipeline, frame, pass.getGraph(), 0);

                if(useRaytracingVersion) {
                    auto& tlas = frame.renderer.getASBuilder().getTopLevelAS();
                    if(tlas) {
                        renderer.bindAccelerationStructure(*resolvePipeline, frame, *tlas, 5, 0);
                        renderer.bindTexture(*resolvePipeline, frame, *blueNoise, 5, 1, nullptr);
                        renderer.bindBuffer(*resolvePipeline, frame, renderer.getASBuilder().getGeometriesBuffer(frame), 5, 2);
                        renderer.bindBuffer(*resolvePipeline, frame, renderer.getASBuilder().getInstancesBuffer(frame), 5, 3);
                    }
                }

                resolvePipeline->bind(pass.getRenderPass(), frame, buffer);
                auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                screenQuadMesh.bind(buffer);
                screenQuadMesh.draw(buffer);
           }
    );
}

void Carrot::Render::PassData::GBuffer::readFrom(Render::GraphBuilder& graph, const GBuffer& other) {
    albedo = graph.read(other.albedo, vk::ImageLayout::eShaderReadOnlyOptimal);
    positions = graph.read(other.positions, vk::ImageLayout::eShaderReadOnlyOptimal);
    viewSpaceNormalTangents = graph.read(other.viewSpaceNormalTangents, vk::ImageLayout::eShaderReadOnlyOptimal);
    flags = graph.read(other.flags, vk::ImageLayout::eShaderReadOnlyOptimal);
    entityID = graph.read(other.entityID, vk::ImageLayout::eShaderReadOnlyOptimal);
    metallicRoughnessVelocityXY = graph.read(other.metallicRoughnessVelocityXY, vk::ImageLayout::eShaderReadOnlyOptimal);
    emissiveVelocityZ = graph.read(other.emissiveVelocityZ, vk::ImageLayout::eShaderReadOnlyOptimal);
    depthStencil = graph.read(other.depthStencil, vk::ImageLayout::eDepthStencilReadOnlyOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);

    // TODO: fix (double read in two != passes result in no 'previousLayout' change
    depthStencil.previousLayout = depthStencil.layout;
    albedo.previousLayout = albedo.layout;
}

void Carrot::Render::PassData::GBuffer::bindInputs(Carrot::Pipeline& pipeline, const Render::Context& frame, const Render::Graph& renderGraph, std::uint32_t setID) const {
    auto& renderer = GetRenderer();
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(albedo, frame.swapchainIndex), setID, 0, nullptr);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(positions, frame.swapchainIndex), setID, 1, nullptr);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(viewSpaceNormalTangents, frame.swapchainIndex), setID, 2, nullptr);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(flags, frame.swapchainIndex), setID, 3, renderer.getVulkanDriver().getNearestSampler());
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(entityID, frame.swapchainIndex), setID, 4, nullptr);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(metallicRoughnessVelocityXY, frame.swapchainIndex), setID, 5, nullptr);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(emissiveVelocityZ, frame.swapchainIndex), setID, 6, nullptr);
    // 7 unused
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(depthStencil, frame.swapchainIndex), setID, 8, nullptr, vk::ImageAspectFlagBits::eDepth, vk::ImageViewType::e2D, 0, vk::ImageLayout::eDepthStencilReadOnlyOptimal);

    Render::Texture::Ref skyboxCubeMap = GetEngine().getSkyboxCubeMap();
    if(!skyboxCubeMap || GetEngine().getSkybox() == Carrot::Skybox::Type::None) {
        skyboxCubeMap = renderer.getBlackCubeMapTexture();
    }
    renderer.bindTexture(pipeline, frame, *skyboxCubeMap, setID, 9, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::eCube);

    renderer.bindSampler(pipeline, frame, renderer.getVulkanDriver().getLinearSampler(), setID, 10);
    renderer.bindSampler(pipeline, frame, renderer.getVulkanDriver().getNearestSampler(), setID, 11);
}
