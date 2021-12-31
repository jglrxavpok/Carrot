//
// Created by jglrxavpok on 31/12/2021.
//

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include "reactphysics3d/mathematics/Transform.h"

namespace Carrot::Math {
    /// Represents a position, non-uniform scale and rotation of an object.
    /// This can be in local space, world space, view space, whatever: this is only a mathematical concept which
    /// interpretation depends on the context.
    struct Transform {
        glm::vec3 position{};
        glm::vec3 scale{1.0f};
        glm::quat rotation = glm::identity<glm::quat>();

        Transform() = default;
        Transform(const Transform&) = default;
        Transform(Transform&&) = default;
        Transform& operator=(const Transform&) = default;

        Transform(const reactphysics3d::Transform&);
        Transform& operator=(const reactphysics3d::Transform&);

        operator reactphysics3d::Transform() const;

        /// A 4x4 homogeneous transformation matrix representing this transform.
        /// A parent Transform can optionally be passed as an argument to represent this transform inside the parent's space.
        [[nodiscard]] glm::mat4 toTransformMatrix(const Transform* parent = nullptr) const;
    };
}
