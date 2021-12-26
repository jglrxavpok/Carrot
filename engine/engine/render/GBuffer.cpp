//
// Created by jglrxavpok on 28/12/2020.
//

#include "GBuffer.h"

Carrot::GBuffer::GBuffer(Carrot::VulkanRenderer& renderer, Carrot::RayTracer& raytracer): renderer(renderer), raytracer(raytracer) {

}

void Carrot::GBuffer::onSwapchainImageCountChange(size_t newCount) {

}

void Carrot::GBuffer::onSwapchainSizeChange(int newWidth, int newHeight) {
    // TODO
}

Carrot::Render::Pass<Carrot::Render::PassData::GBuffer>& Carrot::GBuffer::addGBufferPass(Carrot::Render::GraphBuilder& graph, std::function<void(const Carrot::Render::CompiledPass& pass, const Carrot::Render::Context&, vk::CommandBuffer&)> opaqueCallback) {
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
                                                       {},
                                                       vk::AttachmentLoadOp::eClear,
                                                       clearColor,
                                                       vk::ImageLayout::eColorAttachmentOptimal);

                data.depthStencil = graph.createRenderTarget(renderer.getVulkanDriver().getDepthFormat(),
                                                             {},
                                                             vk::AttachmentLoadOp::eClear,
                                                             clearDepth,
                                                             vk::ImageLayout::eDepthStencilAttachmentOptimal);

                data.positions = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                          {},
                                                          vk::AttachmentLoadOp::eClear,
                                                          positionClear,
                                                          vk::ImageLayout::eColorAttachmentOptimal);

                data.normals = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                        {},
                                                        vk::AttachmentLoadOp::eClear,
                                                        positionClear,
                                                        vk::ImageLayout::eColorAttachmentOptimal);

                data.flags = graph.createRenderTarget(vk::Format::eR32Uint,
                                                      {},
                                                      vk::AttachmentLoadOp::eClear,
                                                      clearIntProperties,
                                                      vk::ImageLayout::eColorAttachmentOptimal);

                data.entityID = graph.createRenderTarget(vk::Format::eR32G32B32A32Uint,
                                                        {},
                                                        vk::AttachmentLoadOp::eClear,
                                                        clearEntityID,
                                                        vk::ImageLayout::eColorAttachmentOptimal);
           },
           [opaqueCallback](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::GBuffer& data, vk::CommandBuffer& buffer){
                opaqueCallback(pass, frame, buffer);
           }
    );
    return opaquePass;
}

Carrot::Render::Pass<Carrot::Render::PassData::GBufferTransparent>& Carrot::GBuffer::addTransparentGBufferPass(Render::GraphBuilder& graph, const Carrot::Render::PassData::GBuffer& opaqueData, std::function<void(const Carrot::Render::CompiledPass&, const Render::Context&, vk::CommandBuffer&)> transparentCallback) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
    auto& transparentPass = graph.addPass<Carrot::Render::PassData::GBufferTransparent>("gbuffer-transparent",
                                                                                        [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::GBufferTransparent>& pass, Carrot::Render::PassData::GBufferTransparent& data)
                                                                                        {

                                                                                            data.transparentOutput = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                                                                                                              {},
                                                                                                                                              vk::AttachmentLoadOp::eClear,
                                                                                                                                              clearColor,
                                                                                                                                              vk::ImageLayout::eColorAttachmentOptimal);

                                                                                            data.depthInput = graph.write(opaqueData.depthStencil, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eDepthStencilReadOnlyOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
                                                                                        },
                                                                                        [transparentCallback](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::GBufferTransparent& data, vk::CommandBuffer& buffer){
                                                                                            transparentCallback(pass, frame, buffer);
                                                                                        }
    );
    return transparentPass;
}

Carrot::Render::Pass<Carrot::Render::PassData::GResolve>& Carrot::GBuffer::addGResolvePass(const Carrot::Render::PassData::GBuffer& opaqueData,
                                                                                           const Carrot::Render::PassData::GBufferTransparent& transparentData,
                                                                                      const Carrot::Render::FrameResource& skyboxOutput,
                                                                                      Carrot::Render::GraphBuilder& graph) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
    return graph.addPass<Carrot::Render::PassData::GResolve>("gresolve",
           [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::GResolve>& pass, Carrot::Render::PassData::GResolve& resolveData)
           {
                // pass.prerecordable = true; TODO: since it depends on Lighting descriptor sets which may change, it is not 100% pre-recordable now
                // TODO (or it should be re-recorded when changes happen)
                resolveData.positions = graph.read(opaqueData.positions, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.normals = graph.read(opaqueData.normals, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.albedo = graph.read(opaqueData.albedo, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.transparent = graph.read(transparentData.transparentOutput, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.depthStencil = graph.read(transparentData.depthInput, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
                resolveData.flags = graph.read(opaqueData.flags, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.skybox = graph.read(skyboxOutput, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.resolved = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                        {},
                                                        vk::AttachmentLoadOp::eClear,
                                                        clearColor,
                                                        vk::ImageLayout::eColorAttachmentOptimal);
           },
           [this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::GResolve& data, vk::CommandBuffer& buffer) {
                ZoneScopedN("CPU RenderGraph GResolve");
                const char* shader = GetCapabilities().supportsRaytracing ? "gResolve-rendergraph-raytracing" : "gResolve-rendergraph-noraytracing";
                auto resolvePipeline = renderer.getOrCreateRenderPassSpecificPipeline(shader, pass.getRenderPass());
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.albedo, frame.swapchainIndex), 0, 0, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.depthStencil, frame.swapchainIndex), 0, 1, nullptr, vk::ImageAspectFlagBits::eDepth);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.positions, frame.swapchainIndex), 0, 2, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.normals, frame.swapchainIndex), 0, 3, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.flags, frame.swapchainIndex), 0, 4, renderer.getVulkanDriver().getNearestSampler());
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.transparent, frame.swapchainIndex), 0, 6, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.skybox, frame.swapchainIndex), 0, 7, nullptr);

                renderer.bindSampler(*resolvePipeline, frame, renderer.getVulkanDriver().getNearestSampler(), 0, 8);
                renderer.bindSampler(*resolvePipeline, frame, renderer.getVulkanDriver().getLinearSampler(), 0, 9);

                resolvePipeline->bind(pass.getRenderPass(), frame, buffer);
                auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                screenQuadMesh.bind(buffer);
                screenQuadMesh.draw(buffer);
           }
    );
}
