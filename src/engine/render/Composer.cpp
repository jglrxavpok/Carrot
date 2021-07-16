//
// Created by jglrxavpok on 09/07/2021.
//

#include "Composer.h"
#include "RenderPass.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/render/resources/Mesh.h"
#include "engine/vulkan/VulkanDriver.h"
#include "engine/render/TextureRepository.h"

namespace Carrot::Render {
    ComposerRegion& Composer::add(const FrameResource& toDraw, float left, float right, float top, float bottom, float z) {
        // graph containing composer pass can be initialised after the 'toDraw' texture has been created
        driver.getTextureRepository().getUsages(toDraw.rootID) |= vk::ImageUsageFlagBits::eSampled;

        auto& r = regions.emplace_back();
        r.toDraw = toDraw;
        r.left = left;
        r.right = right;
        r.top = top;
        r.bottom = bottom;
        return r;
    }

    Pass<ComposerPassData>& Composer::appendPass(GraphBuilder& renderGraph) {
        return renderGraph.addPass<ComposerPassData>("composer",
        [this](Render::GraphBuilder& builder, Render::Pass<ComposerPassData>& pass, ComposerPassData& data) {
            vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,1.0f});
            vk::ClearValue clearDepth = vk::ClearDepthStencilValue{
                    .depth = 1.0f,
                    .stencil = 0
            };
            for(const auto& r : regions) {
                data.elements.emplace_back(r);
                data.elements.back().toDraw = builder.read(r.toDraw, vk::ImageLayout::eShaderReadOnlyOptimal);
            }
            data.color = builder.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                   {},
                                                   vk::AttachmentLoadOp::eClear,
                                                   clearColor,
                                                   vk::ImageLayout::eColorAttachmentOptimal);

            data.depthStencil = builder.createRenderTarget(builder.getVulkanDriver().getDepthFormat(),
                                                           {},
                                                           vk::AttachmentLoadOp::eClear,
                                                           clearDepth,
                                                           vk::ImageLayout::eDepthStencilAttachmentOptimal);
        },
        [](const Render::CompiledPass& pass, const Render::Context& frame, const ComposerPassData& data, vk::CommandBuffer& cmds) {
            ZoneScopedN("CPU RenderGraph Composer");
            auto& renderer = frame.renderer;
            auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("composer-blit", pass.getRenderPass());
            auto& screenQuad = renderer.getFullscreenQuad();
            screenQuad.bind(cmds);

            std::uint32_t index = 0;
            for(const auto& e : data.elements) {
                auto& texture = pass.getGraph().getTexture(e.toDraw, frame.swapchainIndex);
                renderer.bindTexture(*pipeline, frame, texture, 0, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, index);
                index++;
            }

            // fill remaining slots
            for (size_t i = index; i < 16 /* TODO: base on constant inside .json file*/ ; i++) {
                renderer.bindTexture(*pipeline, frame, renderer.getVulkanDriver().getDefaultTexture(), 0, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, i);
            }

            index = 0;
            pipeline->bind(pass.getRenderPass(), frame, cmds);
            for(const auto& e : data.elements) {
                struct Region {
                    float left;
                    float right;
                    float top;
                    float bottom;
                    float depth;
                    std::uint32_t texIndex;
                };

                Region r = {
                    .left = e.left,
                    .right = e.right,
                    .top = e.top,
                    .bottom = e.bottom,
                    .depth = e.depth,
                    .texIndex = index,
                };
                renderer.pushConstantBlock(*pipeline, frame, vk::ShaderStageFlagBits::eVertex, cmds, r);

                screenQuad.draw(cmds);

                index++;
            }
        });
    }
}
