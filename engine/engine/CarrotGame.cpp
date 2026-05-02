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

    for (auto& [id, location] : composition.viewports) {
        Render::Viewport& viewport = engine.getOrCreateViewport(id);

        if (!location.renderGraph) {
            Render::GraphBuilder builder{GetVulkanDriver(), engine.getMainWindow()};
            Render::FrameResource colorOutput = engine.fillGraphBuilderForSingleGameViewport(builder);
            location.renderGraph = builder.compile();
            location.colorTextureExtractor = [colorOutput](Render::Graph& g){ return colorOutput; };
        }

        Render::FrameResource colorTexture = location.colorTextureExtractor(*location.renderGraph);
        viewport.removeAllScenes();
        viewport.addScene(&engine.getSceneManager().getMainScene());
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

void Carrot::CarrotGame::onGameViewportSizeChanged(u32 w, u32 h) {
    for (const auto& [viewportID, location] : currentComposition.viewports) {
        Render::Viewport& viewport = engine.getOrCreateViewport(viewportID);

        // need to be after render graph, to tell render graph about new size
        glm::ivec2 viewportSize = location.size * glm::vec2{w, h};
        verify(viewportSize.x > 0 && viewportSize.y > 0, "Invalid viewport size");
        viewport.resize(viewportSize.x, viewportSize.y);
    }
}
