#include <iostream>
#include <GLFW/glfw3.h>
#include "engine/Engine.h"
#include "engine/constants.h"
#include "stb_image.h"
#include "Game.h"
#include "engine/Configuration.h"

#ifdef TRACY_ENABLE
void* operator new(std::size_t count) {
    auto ptr = malloc(count);
    TracyAllocS(ptr, count, 20);
    return ptr;
}

void operator delete(void* ptr) noexcept{
    TracyFreeS(ptr, 20);
    free(ptr);
}
#endif

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
