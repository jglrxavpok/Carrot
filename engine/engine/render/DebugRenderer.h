//
// Created by jglrxavpok on 20/07/25.
//

#pragma once
#include <core/UniquePtr.hpp>
#include <core/containers/Vector.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "resources/PerFrame.h"
#include "resources/Pipeline.h"

namespace Carrot {
    class SingleMesh;
}

namespace Carrot {
    class VulkanRenderer;
}

namespace Carrot::Render {
    struct Context;

    class DebugRenderer {
    public:
        struct LineDesc {
            glm::vec3 a;
            glm::vec3 b;
            glm::vec4 color;
        };

        explicit DebugRenderer(VulkanRenderer& renderer);
        void render(const Carrot::Render::Context& renderContext);

        /**
         * Draw a world-space line for the current frame
         */
        void drawLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);

        /**
         * Draw a world-space lines for the current frame
         */
        void drawLines(std::span<LineDesc> lines);

    private:
        VulkanRenderer& renderer;
        Vector<LineDesc> lines;
        std::shared_ptr<Pipeline> lineDrawPipeline;
        PerFrame<Vector<std::shared_ptr<SingleMesh>>> meshes; //shared_ptr to avoid including SingleMesh here
    };
} // Render::Carrot