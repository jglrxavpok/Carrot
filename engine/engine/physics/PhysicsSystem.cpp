//
// Created by jglrxavpok on 30/12/2021.
//

#include "PhysicsSystem.h"
#include "engine/utils/Macros.h"
#include "engine/utils/conversions.h"
#include "engine/Engine.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/render/GBufferDrawData.h"
#include "engine/render/InstanceData.h"
#include "engine/render/MaterialSystem.h"
#include "engine/render/resources/Vertex.h"
#include "engine/render/VulkanRenderer.h"
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Renderer/DebugRenderer.h>

using namespace JPH;

// TODO: taken from HelloWorld of JoltPhysics -> adapt

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
{
public:
    virtual bool					ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
            case Carrot::Physics::Layers::NON_MOVING:
                return inObject2 == Carrot::Physics::Layers::MOVING; // Non moving only collides with moving
            case Carrot::Physics::Layers::MOVING:
                return true; // Moving collides with everything
            default:
                JPH_ASSERT(false);
                return false;
        }
    }
};

// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
namespace BroadPhaseLayers
{
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint NUM_LAYERS(2);
};

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[Carrot::Physics::Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Carrot::Physics::Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual uint					GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual BroadPhaseLayer			GetBroadPhaseLayer(ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < Carrot::Physics::Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char *			GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
    {
        switch ((BroadPhaseLayer::Type)inLayer)
        {
            case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
            case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
            default:													JPH_ASSERT(false); return "INVALID";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    BroadPhaseLayer					mObjectToBroadPhase[Carrot::Physics::Layers::NUM_LAYERS];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
{
public:
    virtual bool				ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1)
        {
            case Carrot::Physics::Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Carrot::Physics::Layers::MOVING:
                return true;
            default:
                JPH_ASSERT(false);
                return false;
        }
    }
};

namespace Carrot::Physics {
    PhysicsSystem& PhysicsSystem::getInstance() {
        static PhysicsSystem system;
        return system;
    }

    // TODO: move to class fields

    // Create mapping table from object layer to broadphase layer
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    BPLayerInterfaceImpl broad_phase_layer_interface;

    // Create class that filters object vs broadphase layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;

    // Create class that filters object vs object layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    ObjectLayerPairFilterImpl object_vs_object_layer_filter;

    PhysicsSystem::PhysicsSystem() {
        JPH::RegisterDefaultAllocator();

        Factory::sInstance = new Factory();

        JPH::RegisterTypes();

        // This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
        const uint cMaxBodies = 65536;

        // This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
        const uint cNumBodyMutexes = 0;

        // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
        // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
        // too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
        const uint cMaxBodyPairs = 65536;

        // This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
        // number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
        const uint cMaxContactConstraints = 10240;

        tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
        jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

        jolt = std::make_unique<JPH::PhysicsSystem>();
        jolt->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_vs_object_layer_filter);

        jolt->SetGravity(Carrot::carrotToJolt(glm::vec3{0,0,-9.8f}));
    }

    void PhysicsSystem::tick(double deltaTime) {
        constexpr int collisionSteps = 3;
        constexpr int integrationSteps = 4;
        accumulator += deltaTime;
        while(accumulator >= TimeStep) {
            if(!paused) {
                jolt->Update(TimeStep, collisionSteps, integrationSteps, tempAllocator.get(), jobSystem.get());
            }

            accumulator -= TimeStep;
        }
    }

    void PhysicsSystem::raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, const RaycastCallback& callback, unsigned short collisionMask) const {
        TODO;
    }

    bool PhysicsSystem::raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastInfo& raycastInfo, unsigned short collisionMask) const {
        bool hasAHit = false;
        auto callback = [&](const RaycastInfo& hitInfo) -> float {
            raycastInfo = hitInfo;
            hasAHit = true;
            return 1.0f; // stop at first hit
        };
        raycast(origin, direction, maxDistance, callback, collisionMask);
        return hasAHit;
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

    void PhysicsSystem::onFrame(const Carrot::Render::Context& context) {
        ZoneScoped;

        if(debugViewport != nullptr) {
            if(debugRenderer == nullptr) {
                debugRenderer = std::make_unique<Physics::DebugRenderer>(*debugViewport);
            }
        } else {
            if(debugRenderer != nullptr) {
                static std::size_t framesWithoutRenderer = 0;
                framesWithoutRenderer++;

                if(framesWithoutRenderer >= MAX_FRAMES_IN_FLIGHT) {
                    debugRenderer = nullptr;
                    framesWithoutRenderer = 0;
                }
            }
            return;
        }

        JPH::BodyManager::DrawSettings drawSettings;
        drawSettings.mDrawBoundingBox = true;
        jolt->DrawBodies(drawSettings, debugRenderer.get());
        debugRenderer->render(context);
    }

    Carrot::Render::Viewport* PhysicsSystem::getDebugViewport() {
        return debugViewport;
    }

    void PhysicsSystem::setViewport(Carrot::Render::Viewport *viewport) {
        debugViewport = viewport;
    }

    PhysicsSystem::~PhysicsSystem() {

    }

    JPH::SharedMutex* PhysicsSystem::lockReadBody(const JPH::BodyID& bodyID) {
        return jolt->GetBodyLockInterface().LockRead(bodyID);
    }

    JPH::SharedMutex* PhysicsSystem::lockWriteBody(const JPH::BodyID& bodyID) {
        return jolt->GetBodyLockInterface().LockWrite(bodyID);
    }

    void PhysicsSystem::unlockReadBody(JPH::SharedMutex* mutex) {
        jolt->GetBodyLockInterface().UnlockRead(mutex);
    }

    void PhysicsSystem::unlockWriteBody(JPH::SharedMutex* mutex) {
        jolt->GetBodyLockInterface().UnlockWrite(mutex);
    }

    JPH::Body* PhysicsSystem::lockedGetBody(const JPH::BodyID& bodyID) {
        return jolt->GetBodyLockInterface().TryGetBody(bodyID);
    }

    JPH::BodyID PhysicsSystem::createRigidbody(const JPH::BodyCreationSettings& creationSettings) {
        return jolt->GetBodyInterface().CreateAndAddBody(creationSettings, JPH::EActivation::Activate);
    }

    void PhysicsSystem::destroyRigidbody(const JPH::BodyID& bodyID) {
        jolt->GetBodyInterface().RemoveBody(bodyID);
        jolt->GetBodyInterface().DestroyBody(bodyID);
    }
}