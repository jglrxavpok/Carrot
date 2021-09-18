//
// Created by jglrxavpok on 06/08/2021.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Carrot::ECS {
    struct Kinematics: public IdentifiableComponent<Kinematics> {
        glm::vec3 velocity{};

        explicit Kinematics(Entity entity): IdentifiableComponent<Kinematics>(std::move(entity)) {};
    };
}
