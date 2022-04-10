//
// Created by jglrxavpok on 14/09/2021.
//

#include <engine/CarrotGame.h>
#include <engine/render/Sprite.h>
#include <engine/render/Camera.h>
#include <engine/render/resources/Font.h>

class FontRendering: public Carrot::CarrotGame {
public:

    explicit FontRendering(Carrot::Engine& engine): Carrot::CarrotGame(engine),
    myText(), font(engine.getRenderer(), "resources/fonts/Roboto-Medium.ttf")
    {
        myText = font.bake(U"Test string");
        updateCameraProjection();
    }

    void onFrame(Carrot::Render::Context renderContext) override {
        myText.render(renderContext);
    }

    void tick(double frameTime) override {

    }

    void recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext,
                                 vk::CommandBuffer& commands) override {

    }

    void onSwapchainSizeChange(int newWidth, int newHeight) override {
        updateCameraProjection();
    }

    void updateCameraProjection() {
        auto renderSize = engine.getVulkanDriver().getFinalRenderSize();
        engine.getCamera().setViewProjection(glm::identity<glm::mat4>(), glm::ortho(0.0f, static_cast<float>(renderSize.width), 0.0f, static_cast<float>(renderSize.height), -1.0f, 1.0f));
    }

private:
    Carrot::Render::Font font;
    Carrot::Render::RenderableText myText;
};

void Carrot::Engine::initGame() {
    game = std::make_unique<FontRendering>(*this);
}

int main() {
    std::ios::sync_with_stdio(false);

    Carrot::Configuration config;
    config.requiresRaytracing = false;
    Carrot::Engine engine{config};
    engine.run();

    return 0;
}