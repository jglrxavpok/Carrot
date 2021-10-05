#include <iostream>
#include <GLFW/glfw3.h>
#include "engine/Engine.h"
#include "engine/constants.h"
#include "stb_image.h"
#include "Game.h"
#include "engine/Configuration.h"

int main() {
    std::ios::sync_with_stdio(false);

    Carrot::Configuration config;
#ifdef ENABLE_VR
    config.runInVR = true;
#endif
    config.icon32 = "resources/icon32.png";
    config.icon64 = "resources/icon64.png";
    config.icon128 = "resources/icon128.png";

    Carrot::Engine engine{config};
    engine.run();

    return 0;
}

void Carrot::Engine::initGame() {
    game = std::make_unique<Game::Game>(*this);
}
