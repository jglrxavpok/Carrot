//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Carrot {
    struct Transform: public IdentifiableComponent<Transform> {
        glm::vec3 position{};
        glm::quat rotation{0.0f,0.0f,0.0f,1.0f};

        [[nodiscard]] glm::mat4 toTransformMatrix() const;
    };
}
