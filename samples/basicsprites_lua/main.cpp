//
// Created by jglrxavpok on 14/09/2021.
//

#include <engine/CarrotGame.h>
#include <engine/render/Sprite.h>
#include <engine/render/Camera.h>
#include <engine/scripting/LuaScript.h>

// See 'basicsprites' first
class BasicSprites: public Carrot::CarrotGame {
public:
    const char* LuaScript = R"lua(
position = glm.vec2()
direction = glm.vec2(1, -1)
spriteSize = glm.vec2(128, 128)

function init(engine)
    engineRef = engine
    local renderer = engine:getRenderer()
    mySprite = Carrot.Render.Sprite(renderer, renderer:getOrCreateTexture('../icon128.png'))
    mySprite.size = spriteSize
end

function onFrame(renderContext)
    mySprite:onFrame(renderContext)
end

function tick(frameTime)
    -- make sprite bounce on window bounds
    local moveSpeed = 100.0
    local dpos = direction * (moveSpeed * frameTime)

    mySprite.position = mySprite.position + glm.vec3(dpos.x, dpos.y, 0.0)

    local renderSize = engineRef:getVulkanDriver():getFinalRenderSize()
    if mySprite.position.x + spriteSize.x/2.0 > renderSize.width then
        mySprite.position.x = renderSize.width - spriteSize.x/2.0
        direction.x = -direction.x
    end
    if mySprite.position.x < spriteSize.x/2.0 then
        mySprite.position.x = spriteSize.x/2.0
        direction.x = -direction.x
    end

    if mySprite.position.y + spriteSize.y/2.0 > renderSize.height then
        mySprite.position.y = renderSize.height - spriteSize.y/2.0
        direction.y = -direction.y
    end
    if mySprite.position.y < spriteSize.y/2.0 then
        mySprite.position.y = spriteSize.y/2.0
        direction.y = -direction.y
    end

    mySprite:tick(frameTime)
end

function recordGBufferPass(vkPass, renderContext, vkCommands)
    mySprite:soloGBufferRender(vkPass, renderContext, vkCommands)
end
    )lua";

    explicit BasicSprites(Carrot::Engine& engine): Carrot::CarrotGame(engine),

    script(LuaScript)
    {
        script["init"](engine);
        updateCameraProjection();
    }

    void onFrame(Carrot::Render::Context renderContext) override {
        script["onFrame"](renderContext);
    }

    void tick(double frameTime) override {
        script["tick"](frameTime);
    }

    void recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext,
                                 vk::CommandBuffer& commands) override {
        script["recordGBufferPass"](pass, renderContext, commands);
    }

    void onSwapchainSizeChange(int newWidth, int newHeight) override {
        updateCameraProjection();
    }

    void updateCameraProjection() {
        auto renderSize = engine.getVulkanDriver().getFinalRenderSize();
        engine.getCamera().setViewProjection(glm::identity<glm::mat4>(), glm::ortho(0.0f, static_cast<float>(renderSize.width), 0.0f, static_cast<float>(renderSize.height), -1.0f, 1.0f));
    }

private:
    Carrot::Lua::Script script;
};

void Carrot::Engine::initGame() {
    game = std::make_unique<BasicSprites>(*this);
}

int main() {
    std::ios::sync_with_stdio(false);

    Carrot::Configuration config;
    config.requiresRaytracing = false;
    Carrot::Engine engine{config};
    engine.run();

    return 0;
}