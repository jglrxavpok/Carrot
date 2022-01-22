//
// Created by jglrxavpok on 30/12/2021.
//

#pragma once

#include <reactphysics3d/reactphysics3d.h>

namespace Carrot {
    class Pipeline;

    namespace Render {
        class Viewport;
        struct Context;
    }
}

namespace Carrot::Physics {
    class PhysicsSystem {
    public:
        constexpr static double TimeStep = 1.0 / 60.0;
        static PhysicsSystem& getInstance();

        void tick(double deltaTime);

    public: // debug rendering
        void setViewport(Carrot::Render::Viewport* viewport);
        void onFrame(const Carrot::Render::Context& context);

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

    private: // debug rendering
        std::shared_ptr<Carrot::Pipeline> debugTrianglesPipeline;
        std::shared_ptr<Carrot::Pipeline> debugLinesPipeline;
        Carrot::Render::Viewport* debugViewport = nullptr;
    };
}
