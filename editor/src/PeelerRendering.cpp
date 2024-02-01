//
// Created by jglrxavpok on 31/01/2024.
//
#include <Peeler.h>
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

        engine.getVulkanDriver().getTextureRepository().getUsages(gameTexture.rootID) |= vk::ImageUsageFlagBits::eSampled;
        const auto& gbufferPass = graphBuilder.getPassData<PassData::GBuffer>("gbuffer").value();
        engine.getVulkanDriver().getTextureRepository().getUsages(gbufferPass.entityID.rootID) |= vk::ImageUsageFlagBits::eTransferSrc;

        gameTexture = addOutlinePass(graphBuilder, resolvePassResult);

        struct TransitionData {
            FrameResource textureToTransition;
        };
        graphBuilder.addPass<TransitionData>("transition for ImGui",
            [&](GraphBuilder& graph, Pass<TransitionData>& pass, TransitionData& data) {
                data.textureToTransition = graph.read(gameTexture, vk::ImageLayout::eShaderReadOnlyOptimal);
                pass.rasterized = false;
            },
            [this](const CompiledPass& pass, const Carrot::Render::Context& renderContext, const TransitionData& data, vk::CommandBuffer& cmds) {
                auto& texture = pass.getGraph().getTexture(data.textureToTransition, renderContext.swapchainIndex);
                texture.assumeLayout(vk::ImageLayout::eColorAttachmentOptimal);
                texture.transitionInline(cmds, vk::ImageLayout::eShaderReadOnlyOptimal);
            }
        );

        gameViewport.setRenderGraph(std::move(graphBuilder.compile()));

        gameViewport.getCamera().setTargetAndPosition(glm::vec3(), glm::vec3(2,-5,5));
    }

    Carrot::Render::FrameResource Application::addOutlinePass(GraphBuilder& graphBuilder, const Carrot::Render::FrameResource& finalRenderedImage) {
        const auto& lightingPass = graphBuilder.getPassData<Carrot::Render::PassData::Lighting>("lighting").value();
        auto& outlinePass = graphBuilder.addPass<PassData::PostProcessing>("editor-outlines", [&](GraphBuilder& graph, Pass<PassData::PostProcessing>& pass, PassData::PostProcessing& data) {
            data.postLighting = graph.read(lightingPass.gBuffer.entityID, vk::ImageLayout::eShaderReadOnlyOptimal);
            data.postProcessed = graph.write(finalRenderedImage, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
        },
        [&](const CompiledPass& pass, const Carrot::Render::Context& renderContext, PassData::PostProcessing& data, vk::CommandBuffer& cmds) {
            auto& renderer = renderContext.renderer;
            auto pipeline = renderer.getOrCreatePipelineFullPath("resources/pipelines/outlines.json");
            auto& texture = pass.getGraph().getTexture(data.postLighting, renderContext.swapchainIndex);
            renderer.bindTexture(*pipeline, renderContext, texture, 0, 0, renderer.getVulkanDriver().getNearestSampler());

            auto& fullscreenMesh = renderer.getFullscreenQuad();
            pipeline->bind(pass.getRenderPass(), renderContext, cmds);

            struct ConstantBlock {
                Carrot::UUID selectedEntity;
            };
            ConstantBlock block;
            if(this->selectedIDs.empty()) {
                block.selectedEntity = Carrot::UUID::null();
            } else {
                block.selectedEntity = this->selectedIDs[0];
            }

            renderer.pushConstantBlock("selection", *pipeline, renderContext, vk::ShaderStageFlagBits::eFragment, cmds, block);
            fullscreenMesh.bind(cmds);
            fullscreenMesh.draw(cmds);
        });
        return outlinePass.getData().postProcessed;
    }
}