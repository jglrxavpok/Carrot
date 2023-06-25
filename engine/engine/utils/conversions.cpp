//
// Created by jglrxavpok on 20/12/2020.
//

#include "conversions.h"
#include <glm/gtc/type_ptr.hpp>

namespace Carrot {
    glm::mat4 glmMat4FromAssimp(const aiMatrix4x4& assimpMatrix) {
        return glm::transpose(glm::make_mat4(&assimpMatrix.a1));
    }

    JPH::Vec3 carrotToJolt(const glm::vec3& v) {
        return {v.x, v.y, v.z};
    }

    glm::vec3 joltToCarrot(const JPH::Vec3& v) {
        return {v.GetX(), v.GetY(), v.GetZ()};
    }

    JPH::Quat carrotToJolt(const glm::quat& q) {
        return {q.x, q.y, q.z, q.w};
    }

    glm::quat joltToCarrot(const JPH::Quat& q) {
        return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()};
    }
}
