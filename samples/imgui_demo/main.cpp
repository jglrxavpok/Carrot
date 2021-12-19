//
// Created by jglrxavpok on 14/09/2021.
//

#include <engine/CarrotGame.h>
#include <engine/render/Sprite.h>
#include <engine/render/Camera.h>
#include <engine/scripting/LuaScript.h>

class ImGuiDemo: public Carrot::CarrotGame {
public:
    explicit ImGuiDemo(Carrot::Engine& engine): Carrot::CarrotGame(engine)
    {
    }

    void onFrame(Carrot::Render::Context renderContext) override {
        ImGui::ShowDemoWindow();
    }

    void tick(double frameTime) override {
        // no op
    }
};

void Carrot::Engine::initGame() {
    game = std::make_unique<ImGuiDemo>(*this);
}

int main() {
    std::ios::sync_with_stdio(false);

    Carrot::Configuration config;
    config.requiresRaytracing = false;
    Carrot::Engine engine{config};
    engine.run();

    return 0;
}