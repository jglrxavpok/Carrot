//
// Created by jglrxavpok on 30/12/2021.
//

#pragma once

#include <reactphysics3d/reactphysics3d.h>

namespace Carrot::Physics {
    class PhysicsSystem {
    public:
        constexpr static double TimeStep = 1.0 / 60.0;
        static PhysicsSystem& getInstance();

        void tick(double deltaTime);

    public:
        bool isPaused() const;
        void pause();
        void resume();

    public:
        reactphysics3d::PhysicsCommon& getCommons();
        reactphysics3d::PhysicsWorld& getPhysicsWorld();
        const reactphysics3d::PhysicsWorld& getPhysicsWorld() const;

    private:
        explicit PhysicsSystem();
        ~PhysicsSystem();

    private:
        reactphysics3d::PhysicsCommon physics;
        reactphysics3d::PhysicsWorld* world = nullptr;
        double accumulator = 0.0;
        bool paused = false;
    };
}
