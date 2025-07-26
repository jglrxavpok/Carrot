//
// Created by jglrxavpok on 31/01/2024.
//
#include <Peeler.h>
#include <engine/Engine.h>
#include <engine/render/TextureRepository.h>

using namespace Carrot::Render;

namespace Peeler {
    void Application::setupGameViewport() {
        GraphBuilder graphBuilder(engine.getVulkanDriver(), engine.getMainWindow());

        auto& resolvePassResult = engine.fillInDefaultPipeline(graphBuilder, Eye::NoVR,
                                     [&](const CompiledPass& pass, const Carrot::Render::Context& frame, vk::CommandBuffer& cmds) {
                                         currentScene.world.recordOpaqueGBufferPass(pass.getRenderPass(), frame, cmds);
                                         GetRenderer().recordOpaqueGBufferPass(pass.getRenderPass(), frame, cmds);
                                     },
                                     [&](const CompiledPass& pass, const Carrot::Render::Context& frame, vk::CommandBuffer& cmds) {
                                         currentScene.world.recordTransparentGBufferPass(pass.getRenderPass(), frame, cmds);
                                         GetRenderer().recordTransparentGBufferPass(pass.getRenderPass(), frame, cmds);
                                     });

        engine.getVulkanDriver().getResourceRepository().getTextureUsages(gameTexture.rootID) |= vk::ImageUsageFlagBits::eSampled;
        const auto& gbufferPass = graphBuilder.getPassData<PassData::GBuffer>("gbuffer").value();
        engine.getVulkanDriver().getResourceRepository().getTextureUsages(gbufferPass.entityID.rootID) |= vk::ImageUsageFlagBits::eTransferSrc;

        gameTexture = addOutlinePass(graphBuilder, resolvePassResult);
        gameViewport.setRenderGraph(std::move(graphBuilder.compile()));

        gameViewport.getCamera().setTargetAndPosition(glm::vec3(), glm::vec3(2,-5,5));
    }

    Carrot::Render::FrameResource Application::addOutlinePass(GraphBuilder& graphBuilder, const Carrot::Render::FrameResource& finalRenderedImage) {
        const auto& lightingPass = graphBuilder.getPassData<Carrot::Render::PassData::LightingResources>("lighting").value();
        auto& outlinePass = graphBuilder.addPass<PassData::PostProcessing>("editor-outlines", [&](GraphBuilder& graph, Pass<PassData::PostProcessing>& pass, PassData::PostProcessing& data) {
            data.postLighting = graph.read(lightingPass.gBuffer.entityID, vk::ImageLayout::eShaderReadOnlyOptimal);
            data.postProcessed = graph.write(finalRenderedImage, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
        },
        [&](const CompiledPass& pass, const Carrot::Render::Context& renderContext, PassData::PostProcessing& data, vk::CommandBuffer& cmds) {
            auto& renderer = renderContext.renderer;
            auto pipeline = renderer.getOrCreatePipelineFullPath("resources/pipelines/outlines.pipeline");
            auto& texture = pass.getGraph().getTexture(data.postLighting, renderContext.swapchainIndex);
            renderer.bindTexture(*pipeline, renderContext, texture, 0, 0, renderer.getVulkanDriver().getNearestSampler());

            auto& fullscreenMesh = renderer.getFullscreenQuad();

            struct ConstantBlock {
                Carrot::UUID selectedEntities[16];
            };
            ConstantBlock block;
            std::size_t i = 0;
            auto iter = selectedEntityIDs.begin();
            for(; iter != selectedEntityIDs.end() && i < 16; i++) {
                block.selectedEntities[i] = *iter;
            }
            // fill remaining slots with 0,0,0,1 (not null because that's the default value when rendering content)
            for(;i < 16; i++) {
                block.selectedEntities[i] = Carrot::UUID(0,0,0,1);
            }

            renderer.pushConstantBlock("selection", *pipeline, renderContext, vk::ShaderStageFlagBits::eFragment, cmds, block);
            pipeline->bind(pass.getRenderPass(), renderContext, cmds);
            fullscreenMesh.bind(cmds);
            fullscreenMesh.draw(cmds);
        });
        return outlinePass.getData().postProcessed;
    }
}