//
// Created by jglrxavpok on 29/09/2021.
//

#include <engine/Configuration.h>
#include <engine/Engine.h>
#include "Peeler.h"

int main(int argc, char** argv) {
    Carrot::Configuration config {
            .raytracingSupport = Carrot::RaytracingSupport::Supported,
            .runInVR = false,
            .simplifiedMainRenderGraph = true,
            .applicationName = "Peeler",
            .applicationVersion = VK_MAKE_VERSION(0,0,1),
    };
//    try {
        Carrot::Engine engine{argc, argv, config};
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