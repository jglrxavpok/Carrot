//
// Created by jglrxavpok on 09/07/2021.
//

#pragma once

#include <core/UniquePtr.hpp>

#include "RenderGraph.h"

namespace Carrot::Render {

    struct ViewportLocation {
        glm::vec2 offset; // relative to main viewport size [0-1]
        glm::vec2 size; // relative to main viewport size [0-1]
        float z = 0.0f; // can be used to order viewports that overlap

        std::unique_ptr<Graph> renderGraph; // render graph to render to this viewport. If set to empty, will default to engine's game render graph

        // function to extract the final texture for this viewport, which will be used for the final composition. If renderGraph is set to empty, will be ignored, and default engine's render graph will be used to find the resource
        std::function<Render::FrameResource(Graph&)> colorTextureExtractor;
    };

    class Composer;

    /// Defines the position of multiple viewports inside a single window (can be used for split screen rendering)
    struct ViewportComposition {
        std::unordered_map<Carrot::Identifier, ViewportLocation> viewports;

        /// Default composition: Main viewport takes entire screen
        ViewportComposition() = default;

        /// Copies the viewport list and positions of each viewport from 'other'. Does not copy render graph
        void copyViewportPositions(const ViewportComposition& other);

        /// Set ups the given composer to match what is inside viewports
        void fillComposer(Composer& composer);
    };

    /// Renders multiple textures to a single one. Can be used for content embedding, or axis-aligned split screen
    class Composer {
    public:
        Composer(VulkanDriver& driver): driver(driver) {};

        /// Adds a texture to render. Coordinates are expressed in the NDC (Normalized device coordinate) system
        PassData::ComposerRegion& add(const FrameResource& toDraw, float left = -1.0f, float right = 1.0f, float top = 1.0f, float bottom = -1.0f, float z = 0.0f);

        /// Does this composer have anything to render?
        bool hasRegions() const;

        /// Remove all regions used by this composer
        void clear();

    public:
        Pass<PassData::Composer>& appendPass(GraphBuilder& renderGraph);

    private:
        VulkanDriver& driver;
        std::list<PassData::ComposerRegion> regions;
    };
}
