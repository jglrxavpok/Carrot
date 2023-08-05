//
// Created by jglrxavpok on 29/09/2021.
//

#include <engine/Configuration.h>
#include <engine/Engine.h>
#include "Peeler.h"

int main() {
    Carrot::Configuration config {
            .raytracingSupport = Carrot::RaytracingSupport::NotSupported,
            .runInVR = false,
            .applicationName = "Peeler",
            .applicationVersion = VK_MAKE_VERSION(0,0,1),
    };
//    try {
        Carrot::Engine engine{config};
        engine.run();
/*    } catch(const std::exception& e) {
        std::cerr << "Application died with exception:\n" << e.what() << std::endl;
        throw e;
    }*/
    return 0;
}

void Carrot::Engine::initGame() {
    game = std::make_unique<Peeler::Application>(*this);
}