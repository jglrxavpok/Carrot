//
// Created by jglrxavpok on 06/03/2021.
//

#include "SystemSinPosition.h"

namespace Carrot::ECS {
    void SystemSinPosition::tick(double dt) {
        time += dt;

        forEachEntity([&](Entity& entity, Transform& transform, ForceSinPosition& forceSinPosition) {
            auto sinArguments =
                    forceSinPosition.angularFrequency * ((float) time) + forceSinPosition.angularOffset;
            transform.position =
                    glm::sin(sinArguments) * forceSinPosition.amplitude + forceSinPosition.centerPosition;
        });
    }

    std::unique_ptr<System> SystemSinPosition::duplicate(World& newOwner) const {
        auto system = std::make_unique<SystemSinPosition>(newOwner);
        system->time = time;
        return system;
    }
}