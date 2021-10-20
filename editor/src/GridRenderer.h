//
// Created by jglrxavpok on 19/10/2021.
//

#pragma once

#include "engine/render/resources/Mesh.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/Engine.h"
#include "engine/render/VulkanRenderer.h"

namespace Peeler {
    class GridRenderer {
    public:
        explicit GridRenderer();

        void drawGrid(const vk::RenderPass& renderPass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& cmds,
                      const glm::vec4& color, float linePixelWidth, float cellSize, float size);

    private:
        Carrot::Mesh gridMesh;
        std::shared_ptr<Carrot::Pipeline> renderingPipeline = nullptr;
    };
}
