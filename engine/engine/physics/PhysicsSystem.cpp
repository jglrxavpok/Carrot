//
// Created by jglrxavpok on 30/12/2021.
//

#include "PhysicsSystem.h"
#include <core/Macros.h>

namespace Carrot::Physics {
    PhysicsSystem& PhysicsSystem::getInstance() {
        static PhysicsSystem system;
        return system;
    }

    PhysicsSystem::PhysicsSystem() {
        rp3d::PhysicsWorld::WorldSettings settings;
        settings.gravity = rp3d::Vector3(0, 0, -9.81);
        world = physics.createPhysicsWorld(settings);
    }

    void PhysicsSystem::tick(double deltaTime) {
        accumulator += deltaTime;
        while(accumulator >= TimeStep) {
            if(!paused) {
                world->update(TimeStep);
            }

            accumulator -= TimeStep;
        }
    }

    void PhysicsSystem::pause() {
        paused = true;
    }

    void PhysicsSystem::resume() {
        paused = false;
    }

    bool PhysicsSystem::isPaused() const {
        return paused;
    }

    PhysicsSystem::~PhysicsSystem() {
        physics.destroyPhysicsWorld(world);
    }
}