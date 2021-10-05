//
// Created by jglrxavpok on 03/10/2021.
//

#include <glm/gtx/quaternion.hpp>
#include "FreeCameraController.h"
#include "engine/io/Logging.hpp"

namespace Carrot::Edition {
    void FreeCameraController::move(float strafe, float forward, float upward, float rotDx, float rotDy, double dt) {
        auto epsilonClamp = [](float& f) {
            if(glm::abs(f) < 10e-16) {
                f = 0.0f;
            }
        };

        epsilonClamp(strafe);
        epsilonClamp(forward);
        epsilonClamp(upward);
        epsilonClamp(rotDx);
        epsilonClamp(rotDy);

        eulerAngles.x += rotDy * dt * xSensibility;
        eulerAngles.z += rotDx * dt * ySensibility;

        glm::quat rotationQuat = glm::quat(eulerAngles);
        glm::vec3 movement { strafe * strafeSpeed, -upward * verticalSpeed, forward * forwardSpeed };
        position += glm::rotate(rotationQuat, movement) * static_cast<float>(dt);
    }

    void FreeCameraController::applyTo(const glm::vec2& viewportSize, Carrot::Camera& camera) const {
        glm::quat rotationQuat = glm::quat(eulerAngles);
        glm::mat4 transform = glm::translate(glm::identity<glm::mat4>(), position)
                * glm::toMat4(rotationQuat);
        glm::mat4 view = glm::inverse(transform);

        float aspectRatio = viewportSize.x / viewportSize.y;

        glm::mat4 projection = glm::perspective(fov, aspectRatio, zNear, zFar);

        camera.setViewProjection(view, projection);
    }
}