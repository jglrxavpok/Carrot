//
// Created by jglrxavpok on 03/10/2021.
//

#include <glm/gtx/quaternion.hpp>
#include "FreeCameraController.h"
#include "engine/io/Logging.hpp"
#include "engine/utils/JSON.h"

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
        glm::vec3 movement { strafe * strafeSpeed, 0.0f, forward * forwardSpeed };
        position += glm::rotate(rotationQuat, movement) * static_cast<float>(dt);
        position.z += upward * verticalSpeed * static_cast<float>(dt);
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

    void FreeCameraController::deserialise(const rapidjson::Value& src) {
        strafeSpeed = src["strafeSpeed"].GetFloat();
        forwardSpeed = src["forwardSpeed"].GetFloat();
        verticalSpeed = src["verticalSpeed"].GetFloat();
        xSensibility = src["xSensibility"].GetFloat();
        ySensibility = src["ySensibility"].GetFloat();

        zNear = src["zNear"].GetFloat();
        zFar = src["zFar"].GetFloat();
        fov = src["fov"].GetFloat();

        eulerAngles = JSON::read<3, float>(src["eulerAngles"]);
        position = JSON::read<3, float>(src["position"]);
    }

    rapidjson::Value FreeCameraController::serialise(rapidjson::Document& dest) const {
        rapidjson::Value obj{rapidjson::kObjectType};

        obj.AddMember("strafeSpeed", strafeSpeed, dest.GetAllocator());
        obj.AddMember("forwardSpeed", forwardSpeed, dest.GetAllocator());
        obj.AddMember("verticalSpeed", verticalSpeed, dest.GetAllocator());
        obj.AddMember("xSensibility", xSensibility, dest.GetAllocator());
        obj.AddMember("ySensibility", ySensibility, dest.GetAllocator());

        obj.AddMember("zNear", zNear, dest.GetAllocator());
        obj.AddMember("zFar", zFar, dest.GetAllocator());
        obj.AddMember("fov", fov, dest.GetAllocator());

        obj.AddMember("eulerAngles", JSON::write<3, float>(eulerAngles, dest), dest.GetAllocator());
        obj.AddMember("position", JSON::write<3, float>(position, dest), dest.GetAllocator());

        return obj;
    }
}