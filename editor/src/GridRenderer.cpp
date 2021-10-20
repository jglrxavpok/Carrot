//
// Created by jglrxavpok on 19/10/2021.
//

#include "GridRenderer.h"

namespace Peeler {
    GridRenderer::GridRenderer(): gridMesh(Carrot::Engine::getInstance().getVulkanDriver(),
                                           std::vector<Carrot::SimpleVertex>{
                                                   { { -0.5f, -0.5f, 0.0f } },
                                                   { { 0.5f, -0.5f, 0.0f } },
                                                   { { 0.5f, 0.5f, 0.0f } },
                                                   { { -0.5f, 0.5f, 0.0f } },
                                           },
                                           std::vector<uint32_t>{
                                                   2,1,0,
                                                   3,2,0,
                                           }
                                           ) {
        renderingPipeline = Carrot::Engine::getInstance().getRenderer().getOrCreatePipeline("grid");
    }

    void GridRenderer::drawGrid(const vk::RenderPass& renderPass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& cmds,
                                const glm::vec4& color, float lineWidth, float cellSize, float size) {
        renderingPipeline->bind(renderPass, renderContext, cmds);
        struct {
            glm::vec4 color; // RGBA
            vk::Extent2D screenSize; // in pixels
            float lineWidth = 1.0f; // in pixels
            float cellSize = 1.0f; // in units
            float size = 1.0f; // in units
        } gridData;
        gridData.color = color;
        gridData.screenSize = renderContext.renderer.getVulkanDriver().getFinalRenderSize();
        gridData.lineWidth = lineWidth;
        gridData.cellSize = cellSize;
        gridData.size = size;
        renderContext.renderer.pushConstantBlock("grid", *renderingPipeline, renderContext, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, cmds, gridData);
        gridMesh.bind(cmds);
        gridMesh.draw(cmds);
    }
}