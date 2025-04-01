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
#include <engine/physics/CollisionLayers.h>
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
    class Character;

    namespace {
        // BroadPhaseLayerInterface implementation
        // This defines a mapping between object and broadphase layers.
        class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
        {
            CollisionLayers& layers;

        public:
            // Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
            // a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
            // You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
            // many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
            // your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
            static inline JPH::BroadPhaseLayer StaticLayer{0};
            static inline JPH::BroadPhaseLayer MovingLayer{1};

            BPLayerInterfaceImpl(CollisionLayers& layers);

            virtual JPH::uint GetNumBroadPhaseLayers() const override;

            virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override;
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED
        };

        class ObjectVsBroadPhaseLayerFilterImpl: public JPH::ObjectVsBroadPhaseLayerFilter {
            CollisionLayers& layers;
        public:
            ObjectVsBroadPhaseLayerFilterImpl(CollisionLayers& layers);

            bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override;
        };

        /// Class that determines if two object layers can collide
        class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
            CollisionLayers& layers;
        public:
            ObjectLayerPairFilterImpl(CollisionLayers& layers);

            virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override;
        };

    }

    class PhysicsSystem: public JPH::ContactListener {
    public:
        constexpr static double TimeStep = 1.0 / 60.0;
        static PhysicsSystem& getInstance();

        void tick(double deltaTime, std::function<void()> prePhysicsCallback, std::function<void()> postPhysicsCallback);

    public: // debug rendering
        Carrot::Render::Viewport* getDebugViewport();
        void setViewport(Carrot::Render::Viewport* viewport);
        void onFrame(const Carrot::Render::Context& context);

    public:
        CollisionLayers& getCollisionLayers();
        const CollisionLayers& getCollisionLayers() const;

        CollisionLayerID getDefaultStaticLayer() const;
        CollisionLayerID getDefaultMovingLayer() const;

    public:
        bool isPaused() const;
        void pause();
        void resume();

    public: // queries
        enum class RayCastLayers {
            All,
            StaticOnly,
            DynamicOnly,
        };

        struct RayCastSettings {
            glm::vec3 origin {0.0f};
            glm::vec3 direction {1.0f}; // must be normalized
            float maxLength {0.0f};

            RayCastLayers allowedLayers { RayCastLayers::All };
            /// Should we report collisions against the given layer? By default yes
            std::function<bool(const CollisionLayerID&)> collideAgainstLayer = [](const CollisionLayerID&) { return true; };

            /// Should we report collisions against the given rigidbody? By default yes
            std::function<bool(const RigidBody&)> collideAgainstBody = [](const RigidBody&) { return true; };

            /// Should we report collisions against the given character? By default yes
            std::function<bool(const Character&)> collideAgainstCharacter = [](const Character&) { return true; };
        };

        /**
         * Raycasts into the world, stopping at the first hit
         * @param collisionMask mask for which collider to test against, by default tests for all
         * @return true iff there was a hit
         */
        bool raycast(const RayCastSettings& settings, RaycastInfo& raycastInfo);

    public:
        JPH::SharedMutex* lockReadBody(const JPH::BodyID& bodyID);
        JPH::SharedMutex* lockWriteBody(const JPH::BodyID& bodyID);

        void unlockReadBody(JPH::SharedMutex* mutex);
        void unlockWriteBody(JPH::SharedMutex* mutex);

        // gets the body corresponding to the given bodyID, assuming a lock is already held on that body
        JPH::Body* lockedGetBody(const JPH::BodyID& bodyID);

    public: // ContactListener interface
        void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;

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
        JPH::BodyID createRigidbody(const JPH::BodyCreationSettings& creationSettings);
        void destroyRigidbody(const JPH::BodyID& bodyID);

    private: // debug rendering
        Carrot::Render::Viewport* debugViewport = nullptr;
        std::unique_ptr<Physics::DebugRenderer> debugRenderer;

        CollisionLayers collisionLayers;

        BPLayerInterfaceImpl broadphaseLayerInterface{collisionLayers};
        ObjectVsBroadPhaseLayerFilterImpl objectVsBPFilter{collisionLayers};
        ObjectLayerPairFilterImpl objectLayerPairFilter{collisionLayers};

        friend class Character;
        friend class RigidBody;
        friend class BaseBody;
    };
}
