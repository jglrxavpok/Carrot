//
// Created by jglrxavpok on 29/09/2021.
//

#include <engine/Configuration.h>
#include <engine/Engine.h>
#include "Peeler.h"

int main() {
    Carrot::Configuration config {
            .useRaytracing = false,
            .runInVR = false,
            .applicationName = "Peeler",
            .applicationVersion = VK_MAKE_VERSION(0,0,1),
    };
    Carrot::Engine engine{config};
    engine.run();
    return 0;
}

void Carrot::Engine::initGame() {
    game = std::make_unique<Peeler::Application>(*this);
}