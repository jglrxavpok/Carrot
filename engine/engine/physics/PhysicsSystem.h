//
// Created by jglrxavpok on 30/12/2021.
//

#pragma once

#include <thread>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <glm/glm.hpp>
#include <engine/physics/Types.h>
#include <engine/physics/DebugRenderer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace Carrot {
    class Pipeline;

    namespace Render {
        class Viewport;
        struct Context;
    }
}

namespace Carrot::Physics {

    // TODO: taken from HelloWorld of JoltPhysics -> adapt

    // Layer that objects can be in, determines which other objects it can collide with
    // Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
    // layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
    // but only if you do collision testing).
    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    };

    class PhysicsSystem {
    public:
        constexpr static double TimeStep = 1.0 / 60.0;
        static PhysicsSystem& getInstance();

        void tick(double deltaTime);

    public: // debug rendering
        Carrot::Render::Viewport* getDebugViewport();
        void setViewport(Carrot::Render::Viewport* viewport);
        void onFrame(const Carrot::Render::Context& context);

    public:
        bool isPaused() const;
        void pause();
        void resume();

    public: // queries
        using RaycastCallback = std::function<float(const RaycastInfo& raycastInfo)>;

        void raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, const RaycastCallback& callback, unsigned short collisionMask = 0xFFFF) const;

        /**
         * Raycasts into the world, stopping at the first hit
         * @param collisionMask mask for which collider to test against, by default tests for all
         * @return true iff there was a hit
         */
        bool raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastInfo& raycastInfo, unsigned short collisionMask = 0xFFFF) const;

    private:
        explicit PhysicsSystem();
        ~PhysicsSystem();

    private:
        std::unique_ptr<JPH::PhysicsSystem> jolt;

        // TODO: from HelloWorld example of JoltPhysics, will need to customize & integrate with rest of engine

        // We need a temp allocator for temporary allocations during the physics update. We're
        // pre-allocating 10 MB to avoid having to do allocations during the physics update.
        // B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
        // If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
        // malloc / free.
        std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;

        // We need a job system that will execute physics jobs on multiple threads. Typically
        // you would implement the JobSystem interface yourself and let Jolt Physics run on top
        // of your own job scheduler. JobSystemThreadPool is an example implementation.
        std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;

        double accumulator = 0.0;
        bool paused = false;

    private:
        JPH::SharedMutex* lockReadBody(const JPH::BodyID& bodyID);
        JPH::SharedMutex* lockWriteBody(const JPH::BodyID& bodyID);

        void unlockReadBody(JPH::SharedMutex* mutex);
        void unlockWriteBody(JPH::SharedMutex* mutex);

        // gets the body corresponding to the given bodyID, assuming a lock is already held on that body
        JPH::Body* lockedGetBody(const JPH::BodyID& bodyID);

        JPH::BodyID createRigidbody(const JPH::BodyCreationSettings& creationSettings);
        void destroyRigidbody(const JPH::BodyID& bodyID);

    private: // debug rendering
        Carrot::Render::Viewport* debugViewport = nullptr;
        std::unique_ptr<Physics::DebugRenderer> debugRenderer;

        friend class Character;
        friend class RigidBody;
    };
}
