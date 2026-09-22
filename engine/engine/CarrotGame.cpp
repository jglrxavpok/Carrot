//
// Created by jglrxavpok on 28/04/2026.
//

#include <engine/CarrotGame.h>
#include <engine/Engine.h>

Carrot::Render::FrameResource Carrot::CarrotGame::updateViewportComposition(Render::ViewportComposition&& composition) {
    verify(!GetConfiguration().runInVR, "Multi viewport not supported in VR");

    Render::Composer& composer = engine.getMainComposer();
    composer.clear();

    const i32 windowWidth = static_cast<i32>(engine.getGameViewport().getSizef().x);
    const i32 windowHeight = static_cast<i32>(engine.getGameViewport().getSizef().y);

    Carrot::Vector<Pair<Carrot::Identifier, Render::ViewportLocation>> sortedViewports;
    for (auto& [id, location] : composition.viewports) {
        sortedViewports.emplaceBack(id, std::move(location));
    }

    sortedViewports.sort([](const Pair<Identifier, Render::ViewportLocation>& a, const Pair<Identifier, Render::ViewportLocation>& b) {
       return a.second.renderingOrder < b.second.renderingOrder;
    });

    std::unordered_map<Carrot::Identifier, Render::FrameResource> depthStencils;

    for (auto& [id, location] : sortedViewports) {
        Render::Viewport& viewport = engine.getOrCreateViewport(id);

        viewport.setStencilSettings(location.stencil);

        if (!location.renderGraph) {
            Render::GraphBuilder builder{GetVulkanDriver(), engine.getMainWindow()};
            std::optional<Render::FrameResource> inheritedDepthStencil;
            if (location.inheritDepthStencil.has_value()) {
                if (depthStencils.contains(location.inheritDepthStencil.value())) {
                    inheritedDepthStencil = depthStencils[location.inheritDepthStencil.value()];
                }
            }
            Engine::ViewportFrameResources frameResources = engine.fillGraphBuilderForSingleGameViewport(builder, Render::Eye::NoVR, {}, inheritedDepthStencil);
            depthStencils[id] = frameResources.depthStencil;
            location.renderGraph = builder.compile();
            location.colorTextureExtractor = [frameResources](Render::Graph& g){ return frameResources.colorOutput; };
        }

        Render::FrameResource colorTexture = location.colorTextureExtractor(*location.renderGraph);
        viewport.setScene(&engine.getSceneManager().getMainScene());
        viewport.setRenderGraph(std::move(location.renderGraph));

        // need to be after render graph, to tell render graph about new size
        glm::ivec2 viewportSize = location.size * glm::vec2{windowWidth, windowHeight};
        verify(viewportSize.x > 0 && viewportSize.y > 0, "Invalid viewport size");
        viewport.resize(viewportSize.x, viewportSize.y);

        const float left = location.offset.x * 2 - 1;
        const float right = (location.offset.x + location.size.x) * 2 - 1;
        const float top = (location.offset.y + location.size.y) * 2 - 1;
        const float bottom = location.offset.y * 2 - 1;
        composer.add(colorTexture, left, right, top, bottom, location.z);
    }

    currentComposition.copyViewportPositions(composition);
    return engine.updateGameViewportRenderGraph();
}

Carrot::Render::FrameResource Carrot::CarrotGame::setGameViewport(const Identifier& gameViewportID) {
    Render::Viewport& gameViewport = engine.getOrCreateViewport(gameViewportID);
    engine.setGameViewport(gameViewport);
    return engine.updateGameViewportRenderGraph();
}

bool Carrot::CarrotGame::hasMultipleGameViewports() const {
    return !currentComposition.viewports.empty();
}

void Carrot::CarrotGame::onGameViewportSizeChanged(u32 w, u32 h) {
    for (const auto& [viewportID, location] : currentComposition.viewports) {
        Render::Viewport& viewport = engine.getOrCreateViewport(viewportID);

        // need to be after render graph, to tell render graph about new size
        glm::ivec2 viewportSize = location.size * glm::vec2{w, h};
        verify(viewportSize.x > 0 && viewportSize.y > 0, "Invalid viewport size");
        viewport.resize(viewportSize.x, viewportSize.y);
    }
}
