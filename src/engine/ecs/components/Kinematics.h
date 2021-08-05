//
// Created by jglrxavpok on 06/08/2021.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Carrot {
    struct Kinematics: public IdentifiableComponent<Kinematics> {
        glm::vec3 velocity{};
    };
}
