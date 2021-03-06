//
// Created by jglrxavpok on 06/03/2021.
//

#include "SystemSinPosition.h"

void Carrot::SystemSinPosition::tick(double dt) {
    time += dt;

    for (const auto& e : entities) {
        if(auto entity = e.lock()) {
            auto* transform = world.getComponent<Transform>(entity);
            auto* forceSinPosition = world.getComponent<ForceSinPosition>(entity);

            auto sinArguments = forceSinPosition->angularFrequency * ((float)time) + forceSinPosition->angularOffset;
            transform->position = glm::sin(sinArguments) * forceSinPosition->amplitude + forceSinPosition->centerPosition;
        }
    }
}