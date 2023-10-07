//
// Created by jglrxavpok on 14/09/2021.
//

#include <engine/CarrotGame.h>
#include <engine/render/Model.h>
#include <engine/render/Camera.h>
#include <engine/render/raytracing/ASBuilder.h>
#include <engine/io/actions/Action.hpp>
#include <engine/io/actions/ActionSet.h>
#include <engine/edition/FreeCameraController.h>
#include <engine/render/animation/AnimatedModel.h>
#include "engine/render/animation/AnimatedInstances.h"

constexpr const std::size_t InstanceCount = 10;

class SampleAnimation: public Carrot::CarrotGame {
public:
    explicit SampleAnimation(Carrot::Engine& engine): Carrot::CarrotGame(engine),
                                                      skinnedModel(GetAssetServer().blockingLoadModel("resources/models/unit.gltf")),
                                                      staticModel(GetAssetServer().blockingLoadModel("resources/models/viking_room.obj")),
                                                      skinnedModelRenderer(engine, skinnedModel, InstanceCount),
                                                      animatedModel(skinnedModel)
    {
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

            grabCursor.suggestBinding(Carrot::IO::GLFWMouseButtonBinding(GLFW_MOUSE_BUTTON_RIGHT));
            moveFlashlight.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_F));

            editorActions.add(moveCamera);
            editorActions.add(moveCameraDown);
            editorActions.add(moveCameraUp);
            editorActions.add(turnCamera);
            editorActions.add(grabCursor);
            editorActions.add(moveFlashlight);
            editorActions.activate();
        }

        if(GetCapabilities().supportsRaytracing) {
            auto& asBuilder = GetRenderer().getASBuilder();
            staticBLAS = staticModel->getStaticBLAS();
            staticModelInstance = asBuilder.addInstance(staticBLAS);
        }

        flashlight = GetRenderer().getLighting().create();
        flashlight->light.type = Carrot::Render::LightType::Spot;
        flashlight->light.cutoffCosAngle = glm::cos(glm::pi<float>()/4.0f);
        flashlight->light.outerCutoffCosAngle = glm::cos(glm::pi<float>()/5.0f);
        flashlight->light.enabled = true;

        GetRenderer().getLighting().getAmbientLight() = { 0.1, 0.1, 0.1 };
    }

    void setupCamera(Carrot::Render::Context renderContext) override {
        cameraController.applyTo(renderContext.viewport.getSizef(), renderContext.getCamera());
    }

    void onFrame(Carrot::Render::Context renderContext) override {
        for (int i = 0; i < InstanceCount; ++i) {
            skinnedModelRenderer.getInstance(i).animationTime = time;
            skinnedModelRenderer.getInstance(i).animationIndex = (i+1) % 2;
            skinnedModelRenderer.getInstance(i).transform = glm::translate(glm::identity<glm::mat4>(), glm::vec3(i, 0, 0));
        }
        skinnedModelRenderer.render(renderContext, Carrot::Render::PassEnum::OpaqueGBuffer);

        Carrot::InstanceData staticInstanceData;
        staticModel->renderStatic(renderContext, staticInstanceData, Carrot::Render::PassEnum::OpaqueGBuffer);

        // create and destroy instances rapidly over time, showing that instances can be added and removed easily
        int sizeAtThisTime = (int)((sin(time) / 2.0f + 0.5f) * 30);
        if(sizeAtThisTime >= 0 && animatedModelHandles.size() != sizeAtThisTime) {
            animatedModelHandles.resize(sizeAtThisTime);

            for (int i = 0; i < animatedModelHandles.size(); ++i) {
                auto& pHandle = animatedModelHandles[i];
                if(pHandle == nullptr) { // need more instances compared to previous frame
                    pHandle = animatedModel.requestHandle();
                }

                Carrot::AnimatedInstanceData& instanceData = pHandle->getData();
                instanceData.animationTime = time;
                instanceData.animationIndex = (i+1) % 2;
                instanceData.transform = glm::translate(glm::identity<glm::mat4>(), glm::vec3(i, 5, 0));
            }
        }

        animatedModel.onFrame(renderContext);

        if(moveFlashlight.isPressed()) {
            glm::mat4 invViewMatrix = GetEngine().getCamera().computeViewMatrix();
            invViewMatrix = glm::inverse(invViewMatrix);
            glm::vec4 positionH {0.0f, 0.0f, 0.0f, 1.0f};
            positionH = invViewMatrix * positionH;
            glm::vec4 forwardH {0.0f, 0.0f, -1.0f, 0.0f};
            forwardH = invViewMatrix * forwardH;

            flashlight->light.position = { positionH.x, positionH.y, positionH.z };
            flashlight->light.direction = { forwardH.x, forwardH.y, forwardH.z };
        }
    }

    void tick(double frameTime) override {
        if(grabCursor.isPressed()) {
            engine.grabCursor();
        } else {
            engine.ungrabCursor();
        }

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
        engine.getCamera().setViewProjection(glm::identity<glm::mat4>(), GetEngine().getConfiguration().runInVR ? glm::identity<glm::mat4>() : glm::ortho(0.0f, static_cast<float>(renderSize.width), 0.0f, static_cast<float>(renderSize.height), -1.0f, 1.0f));
    }

private:
    double time = 0.0;
    Carrot::IO::ActionSet editorActions { "Editor actions" };
    Carrot::IO::Vec2InputAction moveCamera { "Move camera (strafe & forward) " };
    Carrot::IO::FloatInputAction moveCameraUp { "Move camera (up) " };
    Carrot::IO::FloatInputAction moveCameraDown { "Move camera (down) " };
    Carrot::IO::Vec2InputAction turnCamera { "Turn camera " };
    Carrot::IO::BoolInputAction grabCursor { "Grab cursor" };
    Carrot::IO::BoolInputAction moveFlashlight { "Move flashlight" };

    Carrot::Edition::FreeCameraController cameraController;

    std::shared_ptr<Carrot::Model> staticModel;
    std::shared_ptr<Carrot::BLASHandle> staticBLAS;
    std::shared_ptr<Carrot::InstanceHandle> staticModelInstance;

    std::shared_ptr<Carrot::Model> skinnedModel;
    Carrot::AnimatedInstances skinnedModelRenderer;

    // simpler interface, intended for ECS
    Carrot::Render::AnimatedModel animatedModel;
    std::vector<std::shared_ptr<Carrot::Render::AnimatedModel::Handle>> animatedModelHandles;

    std::shared_ptr<Carrot::Render::LightHandle> flashlight;
};

void Carrot::Engine::initGame() {
    game = std::make_unique<SampleAnimation>(*this);
}

int main() {
    std::ios::sync_with_stdio(false);

    Carrot::Configuration config;
    config.applicationName = "Animation Sample";
    config.raytracingSupport = Carrot::RaytracingSupport::Supported;
    Carrot::Engine engine{config};
    engine.run();

    return 0;
}