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
        glm::vec3 scale{1.0f};
        glm::quat rotation = glm::identity<glm::quat>();

        explicit Transform(EasyEntity entity): IdentifiableComponent<Transform>(std::move(entity)) {};

        [[nodiscard]] glm::mat4 toTransformMatrix() const;
    };
}
