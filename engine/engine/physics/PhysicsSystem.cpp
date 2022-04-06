//
// Created by jglrxavpok on 30/12/2021.
//

#include "PhysicsSystem.h"
#include "engine/utils/Macros.h"
#include "engine/utils/conversions.h"
#include "engine/Engine.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/render/DrawData.h"
#include "engine/render/InstanceData.h"
#include "engine/render/MaterialSystem.h"
#include "engine/render/resources/Vertex.h"
#include "engine/render/VulkanRenderer.h"

namespace Carrot::Physics {
    struct ReactPhysics3DRaycastCallbackWrapper: public rp3d::RaycastCallback {
        PhysicsSystem::RaycastCallback callbackFunc;

        explicit ReactPhysics3DRaycastCallbackWrapper(const PhysicsSystem::RaycastCallback& callback): callbackFunc(callback) {}

        reactphysics3d::decimal notifyRaycastHit(const reactphysics3d::RaycastInfo& raycastInfo) override {
            return callbackFunc(raycastInfo);
        }
    };

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

    reactphysics3d::PhysicsWorld& PhysicsSystem::getPhysicsWorld() {
        return *world;
    }

    const reactphysics3d::PhysicsWorld& PhysicsSystem::getPhysicsWorld() const {
        return *world;
    }

    void PhysicsSystem::raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, const RaycastCallback& callback) const {
        ReactPhysics3DRaycastCallbackWrapper wrapper { callback };
        rp3d::Vector3 pointA = Carrot::reactPhysicsVecFromGlm(origin);
        rp3d::Vector3 pointB = Carrot::reactPhysicsVecFromGlm(origin + glm::normalize(direction) * maxDistance);

        rp3d::Ray ray { pointA, pointB, 1.0f };

        // TODO: mask
        world->raycast(ray, &wrapper);
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

    reactphysics3d::PhysicsCommon& PhysicsSystem::getCommons() {
        return physics;
    }

    void PhysicsSystem::onFrame(const Carrot::Render::Context& context) {
        if(debugViewport == nullptr)
            return;

        auto& triangles = world->getDebugRenderer().getTriangles();
        Carrot::Render::Packet& renderPacket = GetRenderer().makeRenderPacket(Carrot::Render::PassEnum::TransparentGBuffer, *debugViewport);
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
        renderPacket.indexCount = triangles.size() * 3;

        Carrot::DrawData drawData;
        drawData.materialIndex = GetRenderer().getWhiteMaterial().getSlot();

        Carrot::Render::Packet::PushConstant& pushConstant = renderPacket.addPushConstant();
        pushConstant.id = "drawDataPush";
        pushConstant.stages = vk::ShaderStageFlagBits::eFragment;

        pushConstant.setData(drawData);

        Carrot::InstanceData instance;
        renderPacket.useInstance(instance);

        GetRenderer().render(renderPacket);
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
            world->setIsDebugRenderingEnabled(true);
            world->getDebugRenderer().setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::COLLIDER_AABB, true);
            world->getDebugRenderer().setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::COLLISION_SHAPE, true);
        }
    }

    PhysicsSystem::~PhysicsSystem() {
        physics.destroyPhysicsWorld(world);
    }
}