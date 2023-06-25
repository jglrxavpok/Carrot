//
// Created by jglrxavpok on 31/12/2021.
//

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"

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

    public:
        /// Combines this transform with another to produce a new Transform which will be the composite of this transform, then 'other'.
        Transform operator*(const Transform& other) const;

    public:
        void loadJSON(const rapidjson::Value& json);
        rapidjson::Value toJSON(rapidjson::Document::AllocatorType& json) const;

    public:
        /// Decomposes the given 4x4 matrix and stores the result inside this transform
        void fromMatrix(const glm::mat4& matrix);

        /// A 4x4 homogeneous transformation matrix representing this transform.
        /// A parent Transform can optionally be passed as an argument to represent this transform inside the parent's space.
        [[nodiscard]] glm::mat4 toTransformMatrix(const Transform* parent = nullptr) const;
    };
}
