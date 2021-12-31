//
// Created by jglrxavpok on 31/12/2021.
//

#include "Transform.h"
#include <engine/utils/conversions.h>
#include <glm/glm.hpp>

namespace Carrot::Math {
    Transform::Transform(const reactphysics3d::Transform& physics) {
        *this = physics;
    }

    Transform& Transform::operator=(const reactphysics3d::Transform& physics) {
        position = glmVecFromReactPhysics(physics.getPosition());
        rotation = glmQuatFromReactPhysics(physics.getOrientation());
        return *this;
    }

    Transform::operator reactphysics3d::Transform() const {
        reactphysics3d::Transform result;
        result.setPosition(reactPhysicsVecFromGlm(position));
        result.setOrientation(reactPhysicsQuatFromGlm(rotation));
        return result;
    }

    glm::mat4 Transform::toTransformMatrix(const Transform *parent) const {
        auto modelRotation = glm::toMat4(rotation);
        auto matrix = glm::translate(glm::mat4(1.0f), position) * modelRotation * glm::scale(glm::mat4(1.0f), scale);
        if(parent) {
            return parent->toTransformMatrix() * matrix;
        }
        return matrix;
    }
}