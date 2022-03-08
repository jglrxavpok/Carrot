//
// Created by jglrxavpok on 28/07/2021.
//

#include "RigidBodySystem.h"
#include <engine/vulkan/CustomTracyVulkan.h>
#include <engine/utils/Macros.h>

namespace Carrot::ECS {
    RigidBodySystem::RigidBodySystem(World& world): LogicSystem<TransformComponent, RigidBodyComponent>(world) {

    }

    void RigidBodySystem::tick(double dt) {
        forEachEntity([&](Entity& entity, TransformComponent& transformComponent, RigidBodyComponent& rigidBodyComp) {
            auto transform = rigidBodyComp.rigidbody.getTransform();
            if(rigidBodyComp.firstTick) {
                rigidBodyComp.rigidbody.setTransform(transformComponent.computeGlobalTransform());
                rigidBodyComp.firstTick = false;
            } else {
                transform.scale = transformComponent.computeGlobalTransform().scale; // preserve scale
                transformComponent.setGlobalTransform(transform);
            }

            for(auto& colliderPtr : rigidBodyComp.rigidbody.getColliders()) {
                auto& shape = colliderPtr->getShape();
                switch(shape.getType()) {
                    case Physics::ColliderType::StaticConcaveTriangleMesh: {
                        auto& asMesh = static_cast<Physics::StaticConcaveMeshCollisionShape&>(shape);
                        asMesh.setScale(transform.scale);
                    }
                    break;

                    // TODO: DynamicConvexTriangleMesh
                }
            }
        });
    }

    std::unique_ptr<System> RigidBodySystem::duplicate(World& newOwner) const {
        return std::make_unique<RigidBodySystem>(newOwner);
    }

    void RigidBodySystem::reload() {
        forEachEntity([&](Entity& entity, TransformComponent& transform, RigidBodyComponent& rigidBodyComp) {
            rigidBodyComp.reload();
        });
    }

    void RigidBodySystem::unload() {
        forEachEntity([&](Entity& entity, TransformComponent& transform, RigidBodyComponent& rigidBodyComp) {
            rigidBodyComp.unload();
        });
    }
}
