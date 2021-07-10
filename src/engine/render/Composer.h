//
// Created by jglrxavpok on 09/07/2021.
//

#pragma once

#include "RenderGraph.h"

namespace Carrot::Render {

    struct ComposerRegion {
        bool enabled = true;
        float left = -1.0f;
        float right = +1.0f;
        float bottom = -1.0f;
        float top = +1.0f;
        float depth = 0.0f;
        FrameResource toDraw;
    };

    struct ComposerPassData {
        std::vector<ComposerRegion> elements;
        FrameResource depthStencil;
        FrameResource color;
    };

    /// Renders multiple textures to a single one. Can be used for content embedding, or axis-aligned split screen
    class Composer {
    public:
        Composer(VulkanDriver& driver): driver(driver) {};

        /// Adds a texture to render. Coordinates are expressed in the NDC (Normalized device coordinate) system
        ComposerRegion& add(const FrameResource& toDraw, float left = -1.0f, float right = 1.0f, float top = 1.0f, float bottom = -1.0f, float z = 0.0f);

    public:
        Pass<ComposerPassData>& appendPass(GraphBuilder& renderGraph);

    private:
        VulkanDriver& driver;
        std::list<ComposerRegion> regions;
    };
}
