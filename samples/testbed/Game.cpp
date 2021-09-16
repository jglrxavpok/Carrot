//
// Created by jglrxavpok on 05/12/2020.
//

#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <memory>
#include "Game.h"
#include "engine/Engine.h"
#include "engine/render/Model.h"
#include "engine/render/resources/Buffer.h"
#include "engine/render/Camera.h"
#include "engine/render/resources/Mesh.h"
#include "engine/render/animation/AnimatedInstances.h"
#include <iostream>
#include <engine/render/shaders/ShaderModule.h>
#include <engine/render/raytracing/ASBuilder.h>
#include <engine/render/raytracing/RayTracer.h>

#include <engine/ecs/components/Component.h>
#include <engine/ecs/components/Transform.h>
#include <engine/ecs/components/AnimatedModelInstance.h>
#include <engine/ecs/components/ForceSinPosition.h>
#include <engine/ecs/components/RaycastedShadowsLight.h>
#include <engine/ecs/systems/SystemUpdateAnimatedModelInstance.h>
#include <engine/ecs/systems/SystemSinPosition.h>
#include <engine/ecs/systems/SystemUpdateLightPosition.h>
#include "UnitColor.h"
#include <engine/io/Logging.hpp>

using namespace Carrot;

constexpr static int maxInstanceCount = 100; // TODO: change

static std::vector<size_t> blasIndices{};

Game::Game::Game(Carrot::Engine& engine): CarrotGame(engine) {
    ZoneScoped;

    {
        ZoneScopedN("Define inputs");
        grabMouse.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_G));
        leftClickTest.suggestBinding(Carrot::IO::GLFWMouseButtonBinding(GLFW_MOUSE_BUTTON_LEFT));
        rightClickTest.suggestBinding(Carrot::IO::GLFWMouseButtonBinding(GLFW_MOUSE_BUTTON_RIGHT));
        gamepadButtonTest.suggestBinding(Carrot::IO::GLFWGamepadButtonBinding(GLFW_JOYSTICK_1, GLFW_GAMEPAD_BUTTON_A));
        gamepadLeftTriggerTest.suggestBinding(Carrot::IO::GLFWGamepadAxisBinding(GLFW_JOYSTICK_1, GLFW_GAMEPAD_AXIS_LEFT_TRIGGER));
        gamepadLeftStickTest.suggestBinding(Carrot::IO::GLFWGamepadVec2Binding(GLFW_JOYSTICK_1, Carrot::IO::GameInputVectorType::LeftStick));
        gamepadRightStickTest.suggestBinding(Carrot::IO::GLFWGamepadVec2Binding(GLFW_JOYSTICK_1, Carrot::IO::GameInputVectorType::RightStick));

        cameraMove.suggestBinding(Carrot::IO::GLFWGrabbedMouseDeltaBinding);
        cameraMove.suggestBinding(Carrot::IO::GLFWGamepadVec2Binding(GLFW_JOYSTICK_1, Carrot::IO::GameInputVectorType::RightStick));

        gameplayActions.add(grabMouse);
        gameplayActions.add(leftClickTest);
        gameplayActions.add(rightClickTest);
        gameplayActions.add(gamepadButtonTest);
        gameplayActions.add(gamepadLeftTriggerTest);
        gameplayActions.add(gamepadLeftStickTest);
        gameplayActions.add(gamepadRightStickTest);
        gameplayActions.add(cameraMove);
        gameplayActions.activate();
    }

    engine.setSkybox(Skybox::Type::Forest);

    world.addRenderSystem<SystemUpdateAnimatedModelInstance>();
    world.addLogicSystem<SystemSinPosition>();
    world.addLogicSystem<SystemUpdateLightPosition>();

    for (int i = 0; i < maxInstanceCount * 3; ++i) {
        blasIndices.push_back(i);
    }

    mapModel = std::make_unique<Model>(engine, "resources/models/map/map.obj");
    animatedUnits = std::make_unique<AnimatedInstances>(engine, std::make_shared<Model>(engine, "resources/models/unit.fbx"), maxInstanceCount);

    int groupSize = maxInstanceCount /3;

    mapInstanceBuffer = std::make_unique<Buffer>(engine.getVulkanDriver(),
                                                 sizeof(InstanceData),
                                                 vk::BufferUsageFlagBits::eVertexBuffer,
                                                 vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    auto* mapData = mapInstanceBuffer->map<InstanceData>();
    mapData[0] = {
            {1,1,1,1},
            glm::mat4(1.0f)
    };

    auto& as = engine.getASBuilder();
    const float spacing = 0.5f;
    auto meshes = animatedUnits->getModel().getMeshes();

    std::vector<GeometryInput*> geometries{};
    for(int i = 0; i < maxInstanceCount; i++) {
        float x = (i % (int)sqrt(maxInstanceCount)) * spacing;
        float y = (i / (int)sqrt(maxInstanceCount)) * spacing;
        auto position = glm::vec3(x, y, 0);
        auto color = static_cast<Unit::Type>((i / std::max(1, groupSize)) % 3);

        auto& modelInstance = animatedUnits->getInstance(i);
        auto entity = world.newEntity()
                .addComponent<Transform>()
                .addComponent<UnitColor>(color)
                .addComponent<AnimatedModelInstance>(modelInstance);
        if(auto transform = entity.getComponent<Transform>()) {
            transform->position = position;
            transform->scale = glm::vec3(1.0+i/30.0, 1.0+i/30.0, 1.0+i/30.0);
        }
        switch (color) {
            case Unit::Type::Blue:
                modelInstance.color = {0,0,1,1};
                break;
            case Unit::Type::Red:
                modelInstance.color = {1,0,0,1};
                break;
            case Unit::Type::Green:
                modelInstance.color = {0,1,0,1};
                break;
        }

        for(const auto& mesh : meshes) {
            std::int32_t vertexOffset = animatedUnits->getVertexOffset(i, mesh->getMeshID());

            std::uint64_t vertexStartAddress = vertexOffset * sizeof(Vertex);
            auto g = as.addGeometries<Vertex>(mesh->getBackingBuffer(), mesh->getIndexCount(), 0, animatedUnits->getFullySkinnedBuffer(), mesh->getVertexCount(), {vertexStartAddress});
            geometries.push_back(g);

            auto transform = entity.getComponent<Transform>();
            as.addInstance(InstanceInput {
                .transform = transform->toTransformMatrix(),
                .customInstanceIndex = static_cast<uint32_t>(geometries.size()-1),
                .geometryIndex = static_cast<uint32_t>(geometries.size()-1),
                .mask = 0xFF,
                .hitGroup = 0, // TODO: different materials
            });
        }
    }

    // add map to AS
    auto worldMesh = mapModel->getMeshes()[0];
    as.addGeometries<Vertex>(worldMesh->getBackingBuffer(), worldMesh->getIndexCount(), 0, worldMesh->getBackingBuffer(), worldMesh->getVertexCount(), {worldMesh->getVertexStartOffset()});
    as.addInstance(InstanceInput {
        .customInstanceIndex = static_cast<uint32_t>(geometries.size()),
        .geometryIndex = maxInstanceCount*3, // TODO: don't hardcode
        .mask = 0xFF,
        .hitGroup = 0,
    });

    as.buildBottomLevelAS();
    as.buildTopLevelAS(false, true);

    // TODO: light ID booking system
    auto& light = engine.getRayTracer().getLightBuffer().lights[0];
    auto dynLight = world.newEntity()
            .addComponent<Transform>()
            .addComponent<RaycastedShadowsLight>(light)
            .addComponent<ForceSinPosition>();

    auto* sinPosition = dynLight.getComponent<ForceSinPosition>();
    sinPosition->angularOffset = {0, 1, 2};
    sinPosition->angularFrequency = {1, 2, 3.14};
    sinPosition->amplitude = {1, 2, 0.2};
    sinPosition->centerPosition = {2,2,1};

    auto& sun = engine.getRayTracer().getLightBuffer().lights[1];
    sun.type = Carrot::LightType::Directional;
    sun.color = glm::vec3(1.0, 1.0, 0.995);
    sun.direction = glm::vec3(1, 1, -1);
    sun.intensity = 0.2f;

    world.newEntity()
            .addComponent<RaycastedShadowsLight>(sun);

    // prepare for first frame
    world.tick(0);
    world.onFrame(engine.newRenderContext(0));

    blueprint = std::make_unique<ParticleBlueprint>("resources/particles/testspiral.particle");
    particles = std::make_unique<ParticleSystem>(engine, *blueprint, 10000ul);
    auto emitter = particles->createEmitter();

    float f = sqrt(maxInstanceCount) * spacing / 2.0f;
    emitter->getPosition() = { f, f, 0 };
    emitter->setRate(20);
}

void Game::Game::onFrame(Carrot::Render::Context renderContext) {
    ZoneScoped;
    // TODO: optimize

    auto& skinningSemaphore = animatedUnits->onFrame(renderContext.swapchainIndex);

    world.onFrame(renderContext);

    {
        ZoneScopedN("Update Raytracing AS");
        // TODO: proper indexing
        engine.getASBuilder().updateBottomLevelAS(blasIndices, skinningSemaphore);
        engine.getASBuilder().updateTopLevelAS();
    }

    {
        ZoneScopedN("Prepare particles rendering");
        particles->onFrame(renderContext.swapchainIndex);
    }
}

void Game::Game::recordGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
    ZoneScopedN("CPU recordGBufferPass");
    {
        TracyVulkanZone(*engine.tracyCtx[renderContext.swapchainIndex], commands, "Render map");
        mapModel->draw(pass, renderContext, commands, *mapInstanceBuffer, 1);
    }

    {
        animatedUnits->recordGBufferPass(pass, renderContext, commands, maxInstanceCount);
    }

    particles->gBufferRender(pass, renderContext, commands);
}

float yaw = 0.0f;
float pitch = 0.0f;

void Game::Game::moveCamera(double dx, double dy) {
    auto& camera = engine.getCamera();
    yaw -= dx * 0.1f;// * 0.01f;
    pitch -= dy * 0.1f;// * 0.01f;

    const float distanceFromCenter = 5.0f;
    float cosYaw = cos(yaw);
    float sinYaw = sin(yaw);

    float cosPitch = cos(pitch);
    float sinPitch = sin(pitch);
    camera.getPositionRef() = glm::vec3{cosYaw * cosPitch, sinYaw * cosPitch, sinPitch} * distanceFromCenter;
    camera.getPositionRef() += camera.getTarget();
}

void Game::Game::changeGraphicsWaitSemaphores(uint32_t frameIndex, std::vector<vk::Semaphore>& semaphores, std::vector<vk::PipelineStageFlags>& waitStages) {
/*    semaphores.emplace_back(animatedUnits->getSkinningSemaphore(frameIndex));
    waitStages.emplace_back(vk::PipelineStageFlagBits::eVertexInput);*/
}

static float totalTime = 0.0f;
void Game::Game::tick(double frameTime) {
    if(grabMouse.wasJustPressed()) {
        engine.toggleCursorGrab();
    }
    if(leftClickTest.wasJustPressed()) {
        Carrot::Log::info("left click");
    }
    if(rightClickTest.wasJustPressed()) {
        Carrot::Log::info("right click");
    }
    if(gamepadButtonTest.wasJustPressed()) {
        Carrot::Log::info("gamepad click");
    }
    if(gamepadLeftStickTest.wasJustPressed()) {
        Carrot::Log::info("gamepad left stick non-zero");
    }
    if(gamepadRightStickTest.wasJustPressed()) {
        Carrot::Log::info("gamepad right stick non-zero");
    }
    if(gamepadLeftTriggerTest.wasJustPressed()) {
        Carrot::Log::info("gamepad left trigger pressed");
    }

    if(!engine.getConfiguration().runInVR) {
        const auto& cameraMoveVector = cameraMove.getValue();
        const float deadzone = 0.2f;
        if(glm::length(cameraMoveVector) > deadzone) {
            moveCamera(cameraMoveVector.x, cameraMoveVector.y);
        }
    }

    {
        ZoneScopedN("World tick");
        world.tick(frameTime);
    }

    totalTime += frameTime*10.0f;
    animatedUnits->getInstance(9).color = {
            glm::sin(totalTime)/2.0f+0.5f,
            glm::cos(totalTime)/2.0f+0.5f,
            glm::cos(totalTime)*glm::sin(totalTime)/2.0f+0.5f,
            1.0f,
    };

    {
        ZoneScopedN("Particles tick");
        particles->tick(frameTime);
    }
}

void Game::Game::onSwapchainSizeChange(int newWidth, int newHeight) {
    particles->onSwapchainSizeChange(newWidth, newHeight);
}

void Game::Game::onSwapchainImageCountChange(size_t newCount) {
    particles->onSwapchainImageCountChange(newCount);
}
