//
// Created by jglrxavpok on 05/12/2020.
//

#include <glm/glm.hpp>
#include <memory>
#include "Game.h"
#include "Engine.h"
#include "render/Model.h"
#include "render/Buffer.h"
#include <iostream>

int maxInstanceCount = 100; // TODO: change

Carrot::Game::Game(Carrot::Engine& engine): engine(engine) {
    model = make_unique<Model>(engine, "resources/models/unit.obj");

    int groupSize = maxInstanceCount /3;
    instanceBuffer = make_unique<Buffer>(engine,
                                         maxInstanceCount*sizeof(InstanceData),
                                         vk::BufferUsageFlagBits::eVertexBuffer,
                                         vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    modelInstance = instanceBuffer->map<InstanceData>();

    const float spacing = 0.5f;
    for(int i = 0; i < maxInstanceCount; i++) {
        float x = (i % (int)sqrt(maxInstanceCount)) * spacing;
        float y = (i / (int)sqrt(maxInstanceCount)) * spacing;
        auto position = glm::vec3(x, y, 0);
        auto color = static_cast<Unit::Type>((i / groupSize) % 3);
        auto unit = make_unique<Unit>(color, modelInstance[i]);
        unit->teleport(position);
        units.emplace_back(move(unit));
    }
}

void Carrot::Game::onFrame(uint32_t frameIndex) {
    map<Unit::Type, glm::vec3> centers{};
    map<Unit::Type, uint32_t> counts{};

    for(const auto& unit : units) {
        glm::vec3& currentCenter = centers[unit->getType()];
        counts[unit->getType()]++;
        currentCenter += unit->getPosition();
    }
    centers[Unit::Type::Red] /= counts[Unit::Type::Red];
    centers[Unit::Type::Green] /= counts[Unit::Type::Green];
    centers[Unit::Type::Blue] /= counts[Unit::Type::Blue];

    static double lastTime = glfwGetTime();
    float dt = static_cast<float>(glfwGetTime() - lastTime);
    for(const auto& unit : units) {
        unit->moveTo(centers[unit->getType()]);
        unit->update(dt);
    }
    lastTime = glfwGetTime();
}

void Carrot::Game::recordCommandBuffer(uint32_t frameIndex, vk::CommandBuffer& commands) {
    model->draw(frameIndex, commands, *instanceBuffer, units.size());
}
