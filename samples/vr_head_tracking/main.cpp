//
// Created by jglrxavpok on 18/08/2022.
//

#include <engine/Engine.h>
#include <engine/CarrotGame.h>
#include <engine/render/Model.h>
#include "engine/io/actions/Action.hpp"
#include "engine/io/actions/ActionSet.h"

class SampleVRHead: public Carrot::CarrotGame {
public:
    SampleVRHead(Carrot::Engine& engine): Carrot::CarrotGame(engine),
                                          staticModel(GetRenderer().getOrCreateModel("resources/models/viking_room.obj")){

        goUpInput.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_SPACE));
        goDownInput.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_LEFT_SHIFT));
        actions.add(goUpInput);
        actions.add(goDownInput);
        actions.activate();
    }

    void setupCamera(Carrot::Render::Context renderContext) override {
        // setup camera for each eye
        renderContext.getCamera().getViewMatrixRef() = glm::translate(glm::identity<glm::mat4>(), glm::vec3 { 0.0f, 0.0f, -cameraHeight });
    }

    void onFrame(Carrot::Render::Context renderContext) override {
        // setup rendering
        Carrot::InstanceData instanceData;

        const float scale = 10;
        const float dx = 0.0f;
        const float dy = 0.0f;
        const float dz = -10.0f;
        instanceData.transform = glm::scale(glm::translate(glm::identity<glm::mat4>(), glm::vec3{ dx, dy, dz}), glm::vec3 { scale, scale, scale });
        staticModel->renderStatic(renderContext, instanceData);
    }

    void tick(double frameTime) override {
        float moveSpeed = 10 * frameTime;
        float dz = moveSpeed * (goUpInput.getValue() - goDownInput.getValue());
        cameraHeight += dz;
    }

private:
    float cameraHeight = 0.0f;

    Carrot::IO::ActionSet actions { "Sample actions" };
    Carrot::IO::FloatInputAction goUpInput { "Go Up" };
    Carrot::IO::FloatInputAction goDownInput { "Go Down" };

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