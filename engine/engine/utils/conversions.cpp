//
// Created by jglrxavpok on 20/12/2020.
//

#include "conversions.h"
#include <glm/gtc/type_ptr.hpp>

namespace Carrot {
    glm::mat4 glmMat4FromAssimp(const aiMatrix4x4& assimpMatrix) {
        return glm::transpose(glm::make_mat4(&assimpMatrix.a1));
    }

    JPH::Float2 carrotToJolt(const glm::vec2& v) {
        return {v.x, v.y};
    }

    glm::vec2 joltToCarrot(const JPH::Float2& v) {
        return {v.x, v.y};
    }

    JPH::Vec3 carrotToJolt(const glm::vec3& v) {
        return {v.x, v.y, v.z};
    }

    glm::vec3 joltToCarrot(const JPH::Vec3& v) {
        return {v.GetX(), v.GetY(), v.GetZ()};
    }

    glm::vec3 joltToCarrot(const JPH::Float3& v) {
        return {v.x, v.y, v.z};
    }

    JPH::Quat carrotToJolt(const glm::quat& q) {
        return {q.x, q.y, q.z, q.w};
    }

    glm::vec4 joltToCarrot(const JPH::Vec4& v) {
        return {v.GetX(), v.GetY(), v.GetZ(), v.GetW()};
    }

    glm::vec4 joltToCarrot(const JPH::Float4& v) {
        return {v.x, v.y, v.z, v.w};
    }

    JPH::Vec4 carrotToJolt(const glm::vec4& v) {
        return {v.x, v.y, v.z, v.w};
    }

    glm::quat joltToCarrot(const JPH::Quat& q) {
        return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()};
    }

    JPH::Mat44 carrotToJolt(const glm::mat4& m) {
        JPH::Mat44 out;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                out.SetColumn4(i, carrotToJolt(m[i]));
            }
        }
        return out;
    }

    glm::mat4 joltToCarrot(const JPH::Mat44& m) {
        glm::mat4 out;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                out[i][j] = m.GetColumn4(i)[j];
            }
        }
        return out;
    }
}
