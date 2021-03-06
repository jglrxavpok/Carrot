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

constexpr static int maxInstanceCount = 100; // TODO: change

static vector<size_t> blasIndices{};

Game::Game::Game(Carrot::Engine& engine): CarrotGame(engine) {
    engine.setSkybox(Skybox::Type::Forest);

    world.addRenderSystem<SystemUpdateAnimatedModelInstance>();
    world.addLogicSystem<SystemSinPosition>();
    world.addLogicSystem<SystemUpdateLightPosition>();

    for (int i = 0; i < maxInstanceCount * 3; ++i) {
        blasIndices.push_back(i);
    }

    mapModel = make_unique<Model>(engine, "resources/models/map/map.obj");
    animatedUnits = make_unique<AnimatedInstances>(engine, make_shared<Model>(engine, "resources/models/unit.fbx"), maxInstanceCount);

    int groupSize = maxInstanceCount /3;

    mapInstanceBuffer = make_unique<Buffer>(engine.getVulkanDriver(),
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

    vector<GeometryInput*> geometries{};
    for(int i = 0; i < maxInstanceCount; i++) {
        float x = (i % (int)sqrt(maxInstanceCount)) * spacing;
        float y = (i / (int)sqrt(maxInstanceCount)) * spacing;
        auto position = glm::vec3(x, y, 0);
        auto color = static_cast<Unit::Type>((i / max(1, groupSize)) % 3);

        auto& modelInstance = animatedUnits->getInstance(i);
        auto entity = world.newEntity()
                .addComponent<Transform>()
                .addComponent<UnitColor>(color)
                .addComponent<AnimatedModelInstance>(modelInstance);
        if(auto transform = entity.getComponent<Transform>()) {
            transform->position = position;
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
            int32_t vertexOffset = animatedUnits->getVertexOffset(i, mesh->getMeshID());

            uint64_t vertexStartAddress = vertexOffset * sizeof(Vertex);
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
    as.buildTopLevelAS(false);

    // TODO: light ID booking system
    auto& light = engine.getRayTracer().getLightBuffer().lights[0];
    auto& dynLight = world.newEntity()
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
    world.onFrame(0);

    blueprint = make_unique<ParticleBlueprint>("resources/particles/testspiral.particle");
    particles = make_unique<ParticleSystem>(engine, *blueprint, 10000ul);
    auto emitter = particles->createEmitter();

    float f = sqrt(maxInstanceCount) * spacing / 2.0f;
    emitter->getPosition() = { f, f, 0 };
    emitter->setRate(20);
}

void Game::Game::onFrame(uint32_t frameIndex) {
    ZoneScoped;
    // TODO: optimize

    animatedUnits->onFrame(frameIndex);

/*    map<Unit::Type, glm::vec3> centers{};
    map<Unit::Type, uint32_t> counts{};

    for(const auto& unit : units) {
        glm::vec3& currentCenter = centers[unit->getType()];
        counts[unit->getType()]++;
        currentCenter += unit->getPosition();
    }
    centers[Unit::Type::Red] /= counts[Unit::Type::Red];
    centers[Unit::Type::Green] /= counts[Unit::Type::Green];
    centers[Unit::Type::Blue] /= counts[Unit::Type::Blue];
*/
    world.onFrame(frameIndex);
   // TracyPlot("onFrame delta time", dt*1000);


    // ensure skinning is done
    engine.getComputeQueue().waitIdle();

    // TODO: proper indexing
    engine.getASBuilder().updateBottomLevelAS(blasIndices);
    engine.getASBuilder().updateTopLevelAS();

    particles->onFrame(frameIndex);
}

void Game::Game::recordGBufferPass(vk::RenderPass pass, uint32_t frameIndex, vk::CommandBuffer& commands) {
    {
        TracyVulkanZone(*engine.tracyCtx[frameIndex], commands, "Render map");
        mapModel->draw(pass, frameIndex, commands, *mapInstanceBuffer, 1);
    }

    {
        animatedUnits->recordGBufferPass(pass, frameIndex, commands, maxInstanceCount);
    }

    particles->gBufferRender(pass, frameIndex, commands);
}

float yaw = 0.0f;
float pitch = 0.0f;

void Game::Game::onMouseMove(double dx, double dy) {
    if( ! engine.grabbingCursor())
        return;

    auto& camera = engine.getCamera();
    yaw -= dx * 0.01f;
    pitch -= dy * 0.01f;

    const float distanceFromCenter = 5.0f;
    float cosYaw = cos(yaw);
    float sinYaw = sin(yaw);

    float cosPitch = cos(pitch);
    float sinPitch = sin(pitch);
    camera.position = glm::vec3{cosYaw * cosPitch, sinYaw * cosPitch, sinPitch} * distanceFromCenter;
    camera.position += camera.target;
}

void Game::Game::changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages) {
    semaphores.emplace_back(animatedUnits->getSkinningSemaphore(frameIndex));
    waitStages.emplace_back(vk::PipelineStageFlagBits::eVertexInput);
}

static float totalTime = 0.0f;
void Game::Game::tick(double frameTime) {
    world.tick(frameTime);

    totalTime += frameTime*10.0f;
    animatedUnits->getInstance(99).color = {
            glm::sin(totalTime)/2.0f+0.5f,
            glm::cos(totalTime)/2.0f+0.5f,
            glm::cos(totalTime)*glm::sin(totalTime)/2.0f+0.5f,
            1.0f,
    };

    particles->tick(frameTime);
}

void Game::Game::onSwapchainSizeChange(int newWidth, int newHeight) {
    particles->onSwapchainSizeChange(newWidth, newHeight);
}

void Game::Game::onSwapchainImageCountChange(size_t newCount) {
    particles->onSwapchainImageCountChange(newCount);
}
