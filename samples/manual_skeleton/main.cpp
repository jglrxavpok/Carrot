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
#include "engine/render/animation/AnimatedInstances.h"
#include <engine/render/animation/SkeletalModelRenderer.h>

constexpr const std::size_t InstanceCount = 10;

//! Move a skeleton programmatically (instead of relying on an animation)
class SampleManualSkeleton: public Carrot::CarrotGame {
public:
    explicit SampleManualSkeleton(Carrot::Engine& engine): Carrot::CarrotGame(engine),
                                                      skinnedModel(GetAssetServer().blockingLoadModel("resources/models/cube_tower.fbx")),
                                                      staticModel(GetAssetServer().blockingLoadModel("resources/models/viking_room.obj")),
                                                      renderer(skinnedModel)
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

        // -----
        childBone1 = renderer.getSkeleton().findBone("Child");
        childBone2 = renderer.getSkeleton().findBone("Child.001");
        childBone3 = renderer.getSkeleton().findBone("Child.002");
    }

    void onFrame(Carrot::Render::Context renderContext) override {
        cameraController.applyTo(renderContext.viewport.getSizef(), renderContext.viewport.getCamera());

        { // sample code
            const float angle = glm::sin(static_cast<float>(time) * 2.5f) * 1.0f;
            childBone1->transform = childBone1->originalTransform * glm::rotate(glm::identity<glm::mat4>(), angle, glm::vec3{ 0.0f, 1.0f, 0.0f });
            childBone2->transform = childBone2->originalTransform * glm::rotate(glm::identity<glm::mat4>(), angle, glm::vec3{ 0.0f, 1.0f, 0.0f });
            childBone3->transform = childBone3->originalTransform * glm::rotate(glm::identity<glm::mat4>(), angle, glm::vec3{ 0.0f, 1.0f, 0.0f });

            float scale = 0.01f;
            renderer.getInstanceData().transform = glm::scale(glm::identity<glm::mat4>(), glm::vec3{ scale, scale, scale });

            // upload skeleton + render
            renderer.onFrame(renderContext);
        }

        float staticScale = 10.0f;
        Carrot::InstanceData staticInstanceData;
        staticInstanceData.transform = glm::scale(glm::identity<glm::mat4>(), glm::vec3{ staticScale, staticScale, staticScale });
        staticModelInstance->transform = staticInstanceData.transform;
        staticModel->renderStatic(renderContext, staticInstanceData, Carrot::Render::PassEnum::OpaqueGBuffer);

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
        engine.getCamera().setViewProjection(glm::identity<glm::mat4>(), glm::ortho(0.0f, static_cast<float>(renderSize.width), 0.0f, static_cast<float>(renderSize.height), -1.0f, 1.0f));
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
    std::shared_ptr<Carrot::Render::LightHandle> flashlight;

    std::shared_ptr<Carrot::Model> staticModel;
    std::shared_ptr<Carrot::BLASHandle> staticBLAS;
    std::shared_ptr<Carrot::InstanceHandle> staticModelInstance;
private:
    std::shared_ptr<Carrot::Model> skinnedModel;
    Carrot::Render::SkeletalModelRenderer renderer;

    Carrot::Render::Bone* childBone1 = nullptr;
    Carrot::Render::Bone* childBone2 = nullptr;
    Carrot::Render::Bone* childBone3 = nullptr;
};

void Carrot::Engine::initGame() {
    game = std::make_unique<SampleManualSkeleton>(*this);
}

int main() {
    std::ios::sync_with_stdio(false);

    Carrot::Configuration config;
    config.applicationName = "Manual Skeleton Sample";
    config.raytracingSupport = Carrot::RaytracingSupport::Supported;
    Carrot::Engine engine{config};
    engine.run();

    return 0;
}