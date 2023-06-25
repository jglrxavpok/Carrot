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
// TODO: #include <Jolt/Renderer/DebugRenderer.h>

using namespace JPH;

// TODO: taken from HelloWorld of JoltPhysics -> adapt

// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
// but only if you do collision testing).
namespace Layers
{
    static constexpr ObjectLayer NON_MOVING = 0;
    static constexpr ObjectLayer MOVING = 1;
    static constexpr ObjectLayer NUM_LAYERS = 2;
};

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
{
public:
    virtual bool					ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING; // Non moving only collides with moving
            case Layers::MOVING:
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
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual uint					GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual BroadPhaseLayer			GetBroadPhaseLayer(ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
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
    BroadPhaseLayer					mObjectToBroadPhase[Layers::NUM_LAYERS];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
{
public:
    virtual bool				ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1)
        {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
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
    }

    void PhysicsSystem::tick(double deltaTime) {
        constexpr int collisionSteps = 3;
        constexpr int integrationSteps = 8;
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
        if(debugViewport == nullptr)
            return;

#if 0
        auto& triangles = world->getDebugRenderer().getTriangles();
        Carrot::Render::Packet& renderPacket = GetRenderer().makeRenderPacket(Carrot::Render::PassEnum::OpaqueGBuffer, *debugViewport);
        renderPacket.pipeline = debugTrianglesPipeline;

        Carrot::BufferView vertexBuffer = GetRenderer().getSingleFrameBuffer(triangles.size() * 3 * sizeof(Carrot::Vertex));
        Carrot::BufferView indexBuffer = GetRenderer().getSingleFrameBuffer(triangles.size() * 3 * sizeof(std::uint32_t));

        // TODO: parallelize
        auto* vertices = vertexBuffer.map<Carrot::Vertex>();
        auto* indices = indexBuffer.map<std::uint32_t>();
        for(std::size_t index = 0; index < triangles.size(); index++) {
            auto& triangle = triangles[index];
            auto& vertex1 = vertices[index * 3 + 0];
            auto& vertex2 = vertices[index * 3 + 1];
            auto& vertex3 = vertices[index * 3 + 2];
            vertex1.pos = { triangle.point1.x, triangle.point1.y, triangle.point1.z, 1};
            vertex2.pos = { triangle.point2.x, triangle.point2.y, triangle.point2.z, 1};
            vertex3.pos = { triangle.point3.x, triangle.point3.y, triangle.point3.z, 1};

            vertex1.normal = vertex2.normal = vertex3.normal = {0,0,1};
            vertex1.uv = vertex2.uv = vertex3.uv = { 0, 0 };

            auto toVecColor = [](std::uint32_t color) {
                auto red = (color >> 16) & 0xFF;
                auto green = (color >> 8) & 0xFF;
                auto blue = (color >> 0) & 0xFF;
                return glm::vec3 { red / 255.0f, green / 255.0f, blue / 255.0f };
            };
            vertex1.color = toVecColor(triangle.color1);
            vertex2.color = toVecColor(triangle.color2);
            vertex3.color = toVecColor(triangle.color3);

            indices[index * 3 + 0] = index * 3 + 0;
            indices[index * 3 + 1] = index * 3 + 1;
            indices[index * 3 + 2] = index * 3 + 2;
        }

        renderPacket.vertexBuffer = vertexBuffer;
        renderPacket.indexBuffer = indexBuffer;

        auto& cmd = renderPacket.drawCommands.emplace_back();
        cmd.indexCount = triangles.size() * 3;
        cmd.instanceCount = 1;

        Carrot::GBufferDrawData drawData;
        drawData.materialIndex = GetRenderer().getWhiteMaterial().getSlot();

        Carrot::InstanceData instance;
        renderPacket.useInstance(instance);
        renderPacket.addPerDrawData({&drawData, 1});

        GetRenderer().render(renderPacket);
#endif
        TODO;
    }

    Carrot::Render::Viewport* PhysicsSystem::getDebugViewport() {
        return debugViewport;
    }

    void PhysicsSystem::setViewport(Carrot::Render::Viewport *viewport) {
        debugViewport = viewport;
        if(debugViewport != nullptr) {
            if(!debugTrianglesPipeline) {
                debugTrianglesPipeline = GetRenderer().getOrCreatePipeline("gBufferWireframe");
            }
            if(!debugLinesPipeline) {
                debugLinesPipeline = GetRenderer().getOrCreatePipeline("gBufferLines");
            }
            TODO;
        }
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
        jolt->GetBodyLockInterface().UnlockRead(mutex);
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