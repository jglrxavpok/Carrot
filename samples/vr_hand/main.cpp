//
// Created by jglrxavpok on 18/08/2022.
//

#include <engine/Engine.h>
#include <engine/CarrotGame.h>

class SampleVRHand: public Carrot::CarrotGame {
public:
    SampleVRHand(Carrot::Engine& engine): Carrot::CarrotGame(engine) {

    }

    void onFrame(Carrot::Render::Context renderContext) override {

    }

    void tick(double frameTime) override {

    }
};

void Carrot::Engine::initGame() {
    game = std::make_unique<SampleVRHand>(*this);
}

int main() {
    Carrot::Configuration config;
    config.applicationName = "VR Hand Sample";
    config.raytracingSupport = Carrot::RaytracingSupport::NotSupported;
    config.runInVR = true;

    Carrot::Engine engine{config};
    return 0;
}