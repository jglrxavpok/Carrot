//
// Created by jglrxavpok on 20/12/2020.
//

#include "conversions.h"
#include <glm/gtc/type_ptr.hpp>

namespace Carrot {
    glm::mat4 glmMat4FromAssimp(const aiMatrix4x4& assimpMatrix) {
        return glm::transpose(glm::make_mat4(&assimpMatrix.a1));
    }

    glm::vec2 glmVecFromReactPhysics(const reactphysics3d::Vector2& vec) {
        return { vec.x, vec.y };
    }

    glm::vec3 glmVecFromReactPhysics(const reactphysics3d::Vector3& vec) {
        return { vec.x, vec.y, vec.z };
    }

    glm::quat glmQuatFromReactPhysics(const reactphysics3d::Quaternion& q) {
        return { q.w, q.x, q.y, q.z };
    }

    reactphysics3d::Vector2 reactPhysicsVecFromGlm(const glm::vec2& q) {
        return { q.x, q.y };
    }

    reactphysics3d::Vector3 reactPhysicsVecFromGlm(const glm::vec3& q) {
        return { q.x, q.y, q.z };
    }

    reactphysics3d::Quaternion reactPhysicsQuatFromGlm(const glm::quat& q) {
        return { q.x, q.y, q.z, q.w };
    }
}
