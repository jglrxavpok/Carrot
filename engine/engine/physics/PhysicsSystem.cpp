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
#include "BodyUserData.h"
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Renderer/DebugRenderer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

using namespace JPH;

namespace Carrot::Physics {

    BPLayerInterfaceImpl::BPLayerInterfaceImpl(CollisionLayers& layers): layers(layers) {}

    uint BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const {
        return layers.getCollisionLayersCount();
    }

    BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(ObjectLayer inLayer) const {
        JPH_ASSERT(inLayer < layers.getCollisionLayersCount());
        const auto& layer = layers.getLayer(inLayer);
        if(layer.isStatic) {
            return BPLayerInterfaceImpl::StaticLayer;
        }
        return BPLayerInterfaceImpl::MovingLayer;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* BPLayerInterfaceImpl::GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const {
        if(inLayer == StaticLayer) {
            return "STATIC";
        }
        if(inLayer == MovingLayer) {
            return "MOVING";
        }
        verify(false, "Unknown broadphase layer");
        return "UNKNOWN";
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

    ObjectVsBroadPhaseLayerFilterImpl::ObjectVsBroadPhaseLayerFilterImpl(CollisionLayers& layers): layers(layers) {}

    bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const {
        if(layers.getLayer(inLayer1).isStatic) {
            // static layers don't collide with one another
            return inLayer2 == BPLayerInterfaceImpl::MovingLayer;
        } else {
            return true;
        }
    }

    ObjectLayerPairFilterImpl::ObjectLayerPairFilterImpl(CollisionLayers& layers): layers(layers) {}

    bool ObjectLayerPairFilterImpl::ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const {
        return layers.canCollide(inObject1, inObject2);
    }

    PhysicsSystem& PhysicsSystem::getInstance() {
        static PhysicsSystem system;
        return system;
    }

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
        jolt->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broadphaseLayerInterface, objectVsBPFilter, objectLayerPairFilter);

        jolt->SetGravity(Carrot::carrotToJolt(glm::vec3{0,0,-9.8f}));
    }

    void PhysicsSystem::tick(double deltaTime, std::function<void()> prePhysicsCallback, std::function<void()> postPhysicsCallback) {
        ZoneScoped;
        constexpr int collisionSteps = 3;
        accumulator += deltaTime;
        while(accumulator >= TimeStep) {
            if(!paused) {
                prePhysicsCallback();
                jolt->Update(TimeStep, collisionSteps, tempAllocator.get(), jobSystem.get());
                postPhysicsCallback();
            }

            accumulator -= TimeStep;
        }
    }

    bool PhysicsSystem::raycast(const RayCastSettings& settings, RaycastInfo& raycastInfo) {
        const JPH::RRayCast ray { Carrot::carrotToJolt(settings.origin), Carrot::carrotToJolt(settings.direction * settings.maxLength) };
        JPH::RayCastResult rayResult;
        std::unique_ptr<BroadPhaseLayerFilter> broadphaseLayerFilter;
        switch(settings.allowedLayers) {
            case RayCastLayers::All:
                broadphaseLayerFilter = std::make_unique<BroadPhaseLayerFilter>();
                break;
            case RayCastLayers::StaticOnly:
                broadphaseLayerFilter = std::make_unique<SpecifiedBroadPhaseLayerFilter>(BPLayerInterfaceImpl::StaticLayer);
                break;
            case RayCastLayers::DynamicOnly:
                broadphaseLayerFilter = std::make_unique<SpecifiedBroadPhaseLayerFilter>(BPLayerInterfaceImpl::MovingLayer);
                break;
        }

        struct CustomLayerFilter: public ObjectLayerFilter {
            std::function<bool(const CollisionLayerID&)> collideAgainstLayer;

            CustomLayerFilter(const function<bool(const CollisionLayerID&)> f): collideAgainstLayer(f) {};

            virtual bool ShouldCollide(ObjectLayer inLayer) const {
                return collideAgainstLayer(inLayer);
            }
        };
        CustomLayerFilter objectLayerFilter { settings.collideAgainstLayer };

        struct CustomBodyFilter: public BodyFilter {
            std::function<bool(const RigidBody&)> collideAgainstBody;
            std::function<bool(const Character&)> collideAgainstCharacter;

            CustomBodyFilter(const function<bool(const RigidBody&)> b, const function<bool(const Character&)> c): collideAgainstBody(b), collideAgainstCharacter(c) {};

            struct BodyHandle {
                BodyHandle(const BodyID& inBodyID) {
                    mutex = GetPhysics().lockReadBody(inBodyID);
                    body = GetPhysics().lockedGetBody(inBodyID);
                }

                ~BodyHandle() {
                    GetPhysics().unlockReadBody(mutex);
                }

                JPH::SharedMutex* mutex = nullptr;
                JPH::Body* body = nullptr;
            };

            bool check(const Body& inBody) const {
                BodyUserData* bodyUserData = (BodyUserData*)inBody.GetUserData();
                verify(bodyUserData != nullptr, "No body user data attached to this body??");

                switch(bodyUserData->type) {
                    case BodyUserData::Type::Rigidbody:
                        return collideAgainstBody(*(RigidBody*)bodyUserData->ptr);

                    case BodyUserData::Type::Character:
                        return collideAgainstCharacter(*(Character*)bodyUserData->ptr);
                }
                return false;
            }

            bool ShouldCollide(const BodyID& inBodyID) const override {
                BodyHandle body{ inBodyID };
                return check(*body.body);
            }

            bool ShouldCollideLocked(const Body& inBody) const override {
                return check(inBody);
            }
        };

        CustomBodyFilter bodyFilter { settings.collideAgainstBody, settings.collideAgainstCharacter };
        bool intersected = jolt->GetNarrowPhaseQuery().CastRay(ray, rayResult, *broadphaseLayerFilter, objectLayerFilter, bodyFilter);
        if(intersected) {
            raycastInfo.t = rayResult.mFraction;
            raycastInfo.worldPoint = settings.origin + settings.direction * raycastInfo.t * settings.maxLength;

            JPH::SharedMutex* mutex = lockReadBody(rayResult.mBodyID);
            CLEANUP(unlockReadBody(mutex));
            JPH::Body* body = lockedGetBody(rayResult.mBodyID);
            verify(body != nullptr, "body is null but was collided against??");

            BodyUserData* bodyUserData = (BodyUserData*)body->GetUserData();
            verify(bodyUserData != nullptr, "No body user data attached to this body??");

            raycastInfo.worldNormal = Carrot::joltToCarrot(body->GetWorldSpaceSurfaceNormal(rayResult.mSubShapeID2, ray.GetPointOnRay(rayResult.mFraction)));
            switch(bodyUserData->type) {
                case BodyUserData::Type::Rigidbody:
                    raycastInfo.rigidBody = (RigidBody*)bodyUserData->ptr;
                    break;

                case BodyUserData::Type::Character:
                    raycastInfo.character = (Character*)bodyUserData->ptr;
                    break;
            }
            raycastInfo.collider = nullptr; // TODO
        }
        return intersected;
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

    CollisionLayers& PhysicsSystem::getCollisionLayers() {
        return collisionLayers;
    }

    const CollisionLayers& PhysicsSystem::getCollisionLayers() const {
        return collisionLayers;
    }

    CollisionLayerID PhysicsSystem::getDefaultStaticLayer() const {
        return collisionLayers.getStaticLayer();
    }
    CollisionLayerID PhysicsSystem::getDefaultMovingLayer() const {
        return collisionLayers.getMovingLayer();
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