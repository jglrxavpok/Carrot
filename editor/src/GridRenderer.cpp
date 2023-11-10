//
// Created by jglrxavpok on 19/10/2021.
//

#include "GridRenderer.h"
#include <engine/render/RenderPacket.h>

namespace Peeler {
    GridRenderer::GridRenderer(): gridMesh(std::vector<Carrot::SimpleVertex>{
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

    void GridRenderer::render(const Carrot::Render::Context& renderContext,
                                const glm::vec4& color, float lineWidth, float cellSize, float size) {
        struct {
            glm::vec4 color; // RGBA
            vk::Extent2D screenSize; // in pixels
            float lineWidth = 1.0f; // in pixels
            float cellSize = 1.0f; // in units
            float size = 1.0f; // in units
        } gridData;
        gridData.color = color;
        gridData.screenSize.width = renderContext.pViewport->getWidth();
        gridData.screenSize.height = renderContext.pViewport->getHeight();
        gridData.lineWidth = lineWidth;
        gridData.cellSize = cellSize;
        gridData.size = size;

        Carrot::Render::Packet& renderPacket = GetRenderer().makeRenderPacket(Carrot::Render::PassEnum::OpaqueGBuffer, renderContext);
        renderPacket.pipeline = renderingPipeline;
        renderPacket.useMesh(gridMesh);

        auto& pushConstant = renderPacket.addPushConstant("grid", vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
        pushConstant.setData(gridData);

        renderContext.renderer.render(renderPacket);
    }
}