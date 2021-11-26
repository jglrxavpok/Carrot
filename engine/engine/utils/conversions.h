#pragma once

#include <glm/glm.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <assimp/types.h>

#ifdef ENABLE_VR
#include "engine/vr/includes.h"
#endif

namespace Carrot {
    glm::mat4 glmMat4FromAssimp(const aiMatrix4x4& assimpMatrix);

#ifdef ENABLE_VR
    // From https://github.com/jherico/Vulkan/blob/experimental/openxr/examples/vr_openxr/vr_openxr.cpp
    // (MIT License) Code written by Bradley Austin Davis ("jherico")
    inline XrFovf toTanFovf(const XrFovf& fov) {
        return { tanf(fov.angleLeft), tanf(fov.angleRight), tanf(fov.angleUp), tanf(fov.angleDown) };
    }

    inline glm::mat4 toGlm(const XrFovf& fov, float nearZ = 0.01f, float farZ = 10000.0f) {
        auto tanFov = toTanFovf(fov);
        const auto& tanAngleRight = tanFov.angleRight;
        const auto& tanAngleLeft = tanFov.angleLeft;
        const auto& tanAngleUp = tanFov.angleUp;
        const auto& tanAngleDown = tanFov.angleDown;

        const float tanAngleWidth = tanAngleRight - tanAngleLeft;
        const float tanAngleHeight = (tanAngleDown - tanAngleUp);
        const float offsetZ = 0;

        glm::mat4 resultm{};
        float* result = &resultm[0][0];
        // normal projection
        result[0] = 2 / tanAngleWidth;
        result[4] = 0;
        result[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
        result[12] = 0;

        result[1] = 0;
        result[5] = 2 / tanAngleHeight;
        result[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
        result[13] = 0;

        result[2] = 0;
        result[6] = 0;
        result[10] = -(farZ + offsetZ) / (farZ - nearZ);
        result[14] = -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);

        result[3] = 0;
        result[7] = 0;
        result[11] = -1;
        result[15] = 0;

        return resultm;
    }

    inline glm::quat toGlm(const XrQuaternionf& q) {
        return glm::make_quat(&q.x);
    }

    inline glm::vec3 toGlm(const XrVector3f& v) {
        return glm::make_vec3(&v.x);
    }

    inline glm::mat4 toGlm(const XrPosef& p) {
        glm::mat4 orientation = glm::mat4_cast(toGlm(p.orientation));
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), toGlm(p.position));
        return translation * orientation;
    }

    inline glm::ivec2 toGlm(const XrExtent2Di& e) {
        return { e.width, e.height };
    }
#endif
}