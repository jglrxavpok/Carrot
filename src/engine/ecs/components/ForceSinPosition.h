//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Carrot::ECS {
    struct ForceSinPosition: public IdentifiableComponent<ForceSinPosition> {
        glm::vec3 angularFrequency{1.0f};
        glm::vec3 amplitude{1.0f};
        glm::vec3 angularOffset{0.0f};
        glm::vec3 centerPosition{0.0f};

        explicit ForceSinPosition(Entity entity): IdentifiableComponent<ForceSinPosition>(std::move(entity)) {};
    };
}
