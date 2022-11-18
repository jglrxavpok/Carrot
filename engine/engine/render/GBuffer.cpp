//
// Created by jglrxavpok on 28/12/2020.
//

#include "GBuffer.h"
#include "engine/render/raytracing/ASBuilder.h"

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

                data.depthStencil = graph.createRenderTarget(renderer.getVulkanDriver().getDepthFormat(),
                                                             framebufferSize,
                                                             vk::AttachmentLoadOp::eClear,
                                                             clearDepth,
                                                             vk::ImageLayout::eDepthStencilAttachmentOptimal);

                data.positions = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                          framebufferSize,
                                                          vk::AttachmentLoadOp::eClear,
                                                          positionClear,
                                                          vk::ImageLayout::eColorAttachmentOptimal);

                data.normals = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
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

               data.roughnessMetallic = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                                 framebufferSize,
                                                                 vk::AttachmentLoadOp::eClear,
                                                                 clearColor,
                                                                 vk::ImageLayout::eColorAttachmentOptimal);

               data.emissive = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                        framebufferSize,
                                                        vk::AttachmentLoadOp::eClear,
                                                        clearColor,
                                                        vk::ImageLayout::eColorAttachmentOptimal);
           },
           [opaqueCallback](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::GBuffer& data, vk::CommandBuffer& buffer){
                opaqueCallback(pass, frame, buffer);
           }
    );
    return opaquePass;
}

Carrot::Render::Pass<Carrot::Render::PassData::GBufferTransparent>& Carrot::GBuffer::addTransparentGBufferPass(Render::GraphBuilder& graph, const Carrot::Render::PassData::GBuffer& opaqueData, std::function<void(const Carrot::Render::CompiledPass&, const Render::Context&, vk::CommandBuffer&)> transparentCallback, const Render::TextureSize& framebufferSize) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
    auto& transparentPass = graph.addPass<Carrot::Render::PassData::GBufferTransparent>("gbuffer-transparent",
                                                                                        [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::GBufferTransparent>& pass, Carrot::Render::PassData::GBufferTransparent& data)
                                                                                        {

                                                                                            data.transparentOutput = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                                                                                                              framebufferSize,
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

Carrot::Render::Pass<Carrot::Render::PassData::Lighting>& Carrot::GBuffer::addLightingPass(const Carrot::Render::PassData::GBuffer& opaqueData,
                                                                                           const Carrot::Render::PassData::GBufferTransparent& transparentData,
                                                                                           const Carrot::Render::FrameResource& skyboxOutput,
                                                                                           Carrot::Render::GraphBuilder& graph,
                                                                                           const Render::TextureSize& framebufferSize) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
    return graph.addPass<Carrot::Render::PassData::Lighting>("lighting",
                                                             [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::Lighting>& pass, Carrot::Render::PassData::Lighting& resolveData)
           {
                // pass.prerecordable = true; TODO: since it depends on Lighting descriptor sets which may change, it is not 100% pre-recordable now
                // TODO (or it should be re-recorded when changes happen)
                resolveData.positions = graph.read(opaqueData.positions, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.normals = graph.read(opaqueData.normals, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.albedo = graph.read(opaqueData.albedo, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.transparent = graph.read(transparentData.transparentOutput, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.depthStencil = graph.read(transparentData.depthInput, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
                resolveData.flags = graph.read(opaqueData.flags, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.roughnessMetallic = graph.read(opaqueData.roughnessMetallic, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.emissive = graph.read(opaqueData.emissive, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.skybox = graph.read(skyboxOutput, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.resolved = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                        framebufferSize,
                                                        vk::AttachmentLoadOp::eClear,
                                                        clearColor,
                                                        vk::ImageLayout::eColorAttachmentOptimal);
           },
                                                             [this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::Lighting& data, vk::CommandBuffer& buffer) {
                ZoneScopedN("CPU RenderGraph lighting");
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], buffer, "lighting");
                bool useRaytracingVersion = GetCapabilities().supportsRaytracing;
                if(useRaytracingVersion) {
                    useRaytracingVersion &= !!frame.renderer.getASBuilder().getTopLevelAS();
                }
                const char* shader = useRaytracingVersion ? "lighting-raytracing" : "lighting-noraytracing";
                auto resolvePipeline = renderer.getOrCreateRenderPassSpecificPipeline(shader, pass.getRenderPass());

                struct PushConstant {
                    std::uint32_t frameCount;
                    std::uint32_t frameWidth;
                    std::uint32_t frameHeight;
                } block;

                block.frameCount = renderer.getFrameCount();
                block.frameWidth = GetVulkanDriver().getWindowFramebufferExtent().width;
                block.frameHeight = GetVulkanDriver().getWindowFramebufferExtent().height;
                renderer.pushConstantBlock("push", *resolvePipeline, frame, vk::ShaderStageFlagBits::eFragment, buffer, block);

                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.albedo, frame.swapchainIndex), 0, 0, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.depthStencil, frame.swapchainIndex), 0, 1, nullptr, vk::ImageAspectFlagBits::eDepth);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.positions, frame.swapchainIndex), 0, 2, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.normals, frame.swapchainIndex), 0, 3, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.flags, frame.swapchainIndex), 0, 4, renderer.getVulkanDriver().getNearestSampler());
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.transparent, frame.swapchainIndex), 0, 6, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.roughnessMetallic, frame.swapchainIndex), 0, 7, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.emissive, frame.swapchainIndex), 0, 8, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.skybox, frame.swapchainIndex), 0, 9, nullptr);

                if(useRaytracingVersion) {
                    auto& tlas = frame.renderer.getASBuilder().getTopLevelAS();
                    if(tlas) {
                        renderer.bindAccelerationStructure(*resolvePipeline, frame, *tlas, 0, 12);
                        renderer.bindTexture(*resolvePipeline, frame, *blueNoise, 0, 13, nullptr);
                        renderer.bindBuffer(*resolvePipeline, frame, renderer.getASBuilder().getGeometriesBuffer(frame), 0, 14);
                        renderer.bindBuffer(*resolvePipeline, frame, renderer.getASBuilder().getInstancesBuffer(frame), 0, 15);
                    }
                }

                resolvePipeline->bind(pass.getRenderPass(), frame, buffer);
                auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                screenQuadMesh.bind(buffer);
                screenQuadMesh.draw(buffer);
           }
    );
}
