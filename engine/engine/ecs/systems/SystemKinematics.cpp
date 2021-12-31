//
// Created by jglrxavpok on 06/03/2021.
//

#include "SystemKinematics.h"

namespace Carrot::ECS {
    void SystemKinematics::tick(double dt) {
        forEachEntity([&](Entity& ent, TransformComponent& transform, Kinematics& kinematics) {
            transform.transform.position += kinematics.velocity * static_cast<float>(dt);
        });
    }

    std::unique_ptr<System> SystemKinematics::duplicate(World& newOwner) const {
        return std::make_unique<SystemKinematics>(newOwner);
    }
}