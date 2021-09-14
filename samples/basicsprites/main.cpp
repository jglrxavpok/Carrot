//
// Created by jglrxavpok on 14/09/2021.
//

#include <engine/CarrotGame.h>
#include <engine/render/Sprite.h>
#include <engine/render/Camera.h>

class BasicSprites: public Carrot::CarrotGame {
public:
    const char* TextureName = "../icon128.png"; // Texture names are resolved from 'resources/textures/' (inside the build folder)

    explicit BasicSprites(Carrot::Engine& engine): Carrot::CarrotGame(engine),

    // initialises the sprite, 'getOrCreateTexture' will lazily load a texture given its filename
    mySprite(engine.getRenderer(), engine.getRenderer().getOrCreateTexture(TextureName))

    {
        direction = { 1, -1 };
        updateCameraProjection();
        mySprite.size = spriteSize; // ALL sprites are 1 unit by 1 unit by default
    }

    void onFrame(Carrot::Render::Context renderContext) override {
        mySprite.onFrame(renderContext); // updates underlying instance buffer, and bind texture to sprite pipeline
    }

    void tick(double frameTime) override {
        // make sprite bounce on window bounds
        const float moveSpeed = 100.0f;
        auto dpos = direction * (moveSpeed * static_cast<float>(frameTime));
        mySprite.position += glm::vec3{ dpos.x, dpos.y, 0.0 };

        auto renderSize = engine.getVulkanDriver().getFinalRenderSize();
        if(mySprite.position.x + spriteSize.x/2.0f > renderSize.width) {
            mySprite.position.x = renderSize.width - spriteSize.x/2.0f;
            direction.x = -direction.x;
        }
        if(mySprite.position.x < spriteSize.x/2.0f) {
            mySprite.position.x = spriteSize.x/2.0f;
            direction.x = -direction.x;
        }

        if(mySprite.position.y + spriteSize.y/2.0f > renderSize.height) {
            mySprite.position.y = renderSize.height - spriteSize.y/2.0f;
            direction.y = -direction.y;
        }
        if(mySprite.position.y < spriteSize.y/2.0f) {
            mySprite.position.y = spriteSize.y/2.0f;
            direction.y = -direction.y;
        }

        mySprite.tick(frameTime); // updates the sprite. Does nothing for Sprite, but allows sprite-based animation with AnimatedSprite
    }

    void recordGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext,
                           vk::CommandBuffer& commands) override {
        mySprite.soloGBufferRender(pass, renderContext, commands); // render sprite to GBuffer
    }

    void onSwapchainSizeChange(int newWidth, int newHeight) override {
        updateCameraProjection();
    }

    void updateCameraProjection() {
        auto renderSize = engine.getVulkanDriver().getFinalRenderSize();
        engine.getCamera().setViewProjection(glm::identity<glm::mat4>(), glm::ortho(0.0f, static_cast<float>(renderSize.width), 0.0f, static_cast<float>(renderSize.height), -1.0f, 1.0f));
    }

private:
    Carrot::Render::Sprite mySprite;
    glm::vec2 spriteSize = { 128.0f, 128.0f };
    glm::vec2 direction;
};

void Carrot::Engine::initGame() {
    game = std::make_unique<BasicSprites>(*this);
}

int main() {
    std::ios::sync_with_stdio(false);
    glfwInit();

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    NakedPtr<GLFWwindow> window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);

    // create new scope, as the destructor requires the window to *not* be terminated at its end
    // otherwise this creates a DEP exception when destroying the surface provided by GLFW
    {
        Carrot::Configuration config;
        config.useRaytracing = false;
        Carrot::Engine engine{window, config};
        engine.run();
    }

    glfwDestroyWindow(window.get());
    glfwTerminate();
    return 0;
}