//
// Created by jglrxavpok on 12/11/2023.
//

#pragma once

#include <engine/render/RenderGraph.h>
#include <engine/render/RenderPassData.h>

namespace Carrot {
    class VulkanRenderer;
}

namespace Carrot::Render {

    class VisibilityBuffer {
    public:
        struct VisibilityPassData {
            PassData::GBuffer gbuffer;
            Render::FrameResource visibilityBuffer;
            Render::FrameResource debugView;
        };

        explicit VisibilityBuffer(VulkanRenderer& renderer);

        /**
         * Adds the visibility passes to the given graph.
         * The last step of the visibility passes is to write to the gbuffer, that's why it needs the GBuffer render pass data
         */
        const VisibilityPassData& addVisibilityBufferPasses(Render::GraphBuilder& graph, const Render::PassData::GBuffer& gBufferData, const Render::TextureSize& framebufferSize);

    private:
        VulkanRenderer& renderer;
    };

} // Carrot::Render
