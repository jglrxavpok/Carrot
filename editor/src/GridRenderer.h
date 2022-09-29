//
// Created by jglrxavpok on 19/10/2021.
//

#pragma once

#include "engine/render/resources/SingleMesh.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/Engine.h"
#include "engine/render/VulkanRenderer.h"

namespace Peeler {
    class GridRenderer {
    public:
        explicit GridRenderer();

        void render(const Carrot::Render::Context& renderContext, const glm::vec4& color, float lineWidth, float cellSize, float size);

    private:
        Carrot::SingleMesh gridMesh;
        std::shared_ptr<Carrot::Pipeline> renderingPipeline = nullptr;
    };
}
