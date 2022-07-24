//
// Created by jglrxavpok on 14/09/2021.
//

#include <engine/CarrotGame.h>
#include <engine/render/Model.h>
#include <engine/render/Camera.h>
#include <engine/io/actions/Action.hpp>
#include <engine/io/actions/ActionSet.h>
#include <engine/edition/FreeCameraController.h>
#include "engine/render/animation/AnimatedInstances.h"

class SampleAnimation: public Carrot::CarrotGame {
public:
    explicit SampleAnimation(Carrot::Engine& engine): Carrot::CarrotGame(engine),
                                                      skinnedModel(engine.getRenderer().getOrCreateModel("resources/models/unit.fbx")),
                                                      staticModel(engine.getRenderer().getOrCreateModel("resources/models/viking_room.obj")),
                                                      skinnedModelRenderer(engine, skinnedModel, 1)
    {
        engine.setSkybox(Carrot::Skybox::Type::Forest);
        {
            moveCamera.suggestBinding(Carrot::IO::GLFWGamepadVec2Binding(0, Carrot::IO::GameInputVectorType::LeftStick));
            moveCamera.suggestBinding(Carrot::IO::GLFWKeysVec2Binding(Carrot::IO::GameInputVectorType::WASD));

            turnCamera.suggestBinding(Carrot::IO::GLFWGamepadVec2Binding(0, Carrot::IO::GameInputVectorType::RightStick));
            turnCamera.suggestBinding(Carrot::IO::GLFWKeysVec2Binding(Carrot::IO::GameInputVectorType::ArrowKeys));
            turnCamera.suggestBinding(Carrot::IO::GLFWGrabbedMouseDeltaBinding);

            moveCameraUp.suggestBinding(Carrot::IO::GLFWGamepadAxisBinding(0, GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER));
            moveCameraUp.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_SPACE));

            moveCameraDown.suggestBinding(Carrot::IO::GLFWGamepadAxisBinding(0, GLFW_GAMEPAD_AXIS_LEFT_TRIGGER));
            moveCameraDown.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_LEFT_SHIFT));

            editorActions.add(moveCamera);
            editorActions.add(moveCameraDown);
            editorActions.add(moveCameraUp);
            editorActions.add(turnCamera);
            editorActions.activate();
        }
    }

    void onFrame(Carrot::Render::Context renderContext) override {
        cameraController.applyTo(renderContext.viewport.getSizef(), renderContext.viewport.getCamera());

        skinnedModelRenderer.getInstance(0).animationTime = time;
        skinnedModelRenderer.getInstance(0).animationIndex = 1;
        skinnedModelRenderer.getInstance(0).transform = glm::scale(glm::identity<glm::mat4>(), glm::vec3(10, 10, 10));
        skinnedModelRenderer.render(renderContext, Carrot::Render::PassEnum::OpaqueGBuffer);

        Carrot::InstanceData staticInstanceData;
        staticModel->renderStatic(renderContext, staticInstanceData, Carrot::Render::PassEnum::OpaqueGBuffer);
    }

    void tick(double frameTime) override {
        engine.grabCursor();

        time += frameTime;

        cameraController.move(moveCamera.getValue().x, moveCamera.getValue().y, moveCameraUp.getValue() - moveCameraDown.getValue(),
                              turnCamera.getValue().x, turnCamera.getValue().y,
                              frameTime);
    }

    void recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext,
                                 vk::CommandBuffer& commands) override {

    }

    void onSwapchainSizeChange(int newWidth, int newHeight) override {
        updateCameraProjection();
    }

    void updateCameraProjection() {
        auto renderSize = engine.getVulkanDriver().getFinalRenderSize();
        engine.getCamera().setViewProjection(glm::identity<glm::mat4>(), glm::ortho(0.0f, static_cast<float>(renderSize.width), 0.0f, static_cast<float>(renderSize.height), -1.0f, 1.0f));
    }

private:
    double time = 0.0;
    Carrot::IO::ActionSet editorActions { "Editor actions" };
    Carrot::IO::Vec2InputAction moveCamera { "Move camera (strafe & forward) " };
    Carrot::IO::FloatInputAction moveCameraUp { "Move camera (up) " };
    Carrot::IO::FloatInputAction moveCameraDown { "Move camera (down) " };
    Carrot::IO::Vec2InputAction turnCamera { "Turn camera " };

    Carrot::Edition::FreeCameraController cameraController;

    std::shared_ptr<Carrot::Model> staticModel;
    std::shared_ptr<Carrot::Model> skinnedModel;
    Carrot::AnimatedInstances skinnedModelRenderer;
    glm::vec2 spriteSize = { 128.0f, 128.0f };
    glm::vec2 direction;
};

void Carrot::Engine::initGame() {
    game = std::make_unique<SampleAnimation>(*this);
}

int main() {
    std::ios::sync_with_stdio(false);

    Carrot::Configuration config;
    config.raytracingSupport = Carrot::RaytracingSupport::NotSupported;
    Carrot::Engine engine{config};
    engine.run();

    return 0;
}