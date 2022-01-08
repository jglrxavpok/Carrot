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
        forEachEntity([&](Entity& entity, TransformComponent& transform, RigidBodyComponent& rigidBodyComp) {
            if(rigidBodyComp.firstTick) {
                rigidBodyComp.rigidbody.setTransform(transform.computeGlobalTransform());
                rigidBodyComp.firstTick = false;
            }
            transform.setGlobalTransform(rigidBodyComp.rigidbody.getTransform());

            // TODO: handle collider updates
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
