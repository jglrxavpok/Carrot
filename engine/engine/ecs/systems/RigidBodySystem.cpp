//
// Created by jglrxavpok on 28/07/2021.
//

#include "RigidBodySystem.h"
#include <engine/vulkan/CustomTracyVulkan.h>
#include <engine/utils/Macros.h>
#include <engine/ecs/World.h>

namespace Carrot::ECS {
    RigidBodySystem::RigidBodySystem(World& world): LogicSystem<TransformComponent, RigidBodyComponent>(world) {

    }

    void RigidBodySystem::tick(double dt) {
        forEachEntity([&](Entity& entity, TransformComponent& transformComponent, RigidBodyComponent& rigidBodyComp) {
            rigidBodyComp.rigidbody.setUserData((void *) &entity.getID());

            auto transform = rigidBodyComp.rigidbody.getTransform();
            if(rigidBodyComp.firstTick) {
                rigidBodyComp.rigidbody.setTransform(transformComponent.computeGlobalPhysicsTransform());
                rigidBodyComp.firstTick = false;
            } else {
                transform.scale = transformComponent.computeGlobalPhysicsTransform().scale; // preserve scale
                transformComponent.setGlobalTransform(transform);
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

    /*static*/ std::optional<Entity> RigidBodySystem::entityFromBody(const Carrot::ECS::World& world, const Physics::RigidBody& body) {
        void* pData = body.getUserData();
        if(!pData) {
            return {};
        }

        const EntityID* pID = (EntityID*) pData;
        return world.wrap(*pID);
    }
}
