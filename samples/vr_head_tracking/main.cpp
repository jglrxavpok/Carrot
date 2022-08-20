//
// Created by jglrxavpok on 18/08/2022.
//

#include <engine/Engine.h>
#include <engine/CarrotGame.h>
#include <engine/render/Model.h>

class SampleVRHead: public Carrot::CarrotGame {
public:
    SampleVRHead(Carrot::Engine& engine): Carrot::CarrotGame(engine),
                                          staticModel(GetRenderer().getOrCreateModel("resources/models/viking_room.obj")){

    }

    void onFrame(Carrot::Render::Context renderContext) override {
        Carrot::InstanceData instanceData;

        const float scale = 10;
        const float dx = 0.0f;
        const float dy = 0.0f;
        const float dz = -10.0f;
        instanceData.transform = glm::scale(glm::translate(glm::identity<glm::mat4>(), glm::vec3{ dx, dy, dz}), glm::vec3 { scale, scale, scale });
        staticModel->renderStatic(renderContext, instanceData);
    }

    void tick(double frameTime) override {

    }

private:
    Carrot::Model::Ref staticModel; // have something to render
};

void Carrot::Engine::initGame() {
    game = std::make_unique<SampleVRHead>(*this);
}

int main() {
    Carrot::Configuration config;
    config.applicationName = "VR Head Tracking Sample";
    config.raytracingSupport = Carrot::RaytracingSupport::NotSupported;
    config.runInVR = true;

    Carrot::Engine engine{config};
    engine.run();
    return 0;
}