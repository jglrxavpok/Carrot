//
// Created by jglrxavpok on 03/10/2021.
//

#pragma once

#include <engine/render/Camera.h>
#include <rapidjson/document.h>

namespace Carrot::Edition {
    class FreeCameraController {
    public: // camera definition
        glm::vec3 position{};
        glm::vec3 eulerAngles{ glm::radians(-90.0f), 0.0f, 0.0f };
        float fov = glm::radians(70.0f);
        float zNear = 0.001f;
        float zFar = 1000.0f;

    public: // movement parameters
        float strafeSpeed = 1.0f;
        float forwardSpeed = 1.0f;
        float verticalSpeed = 1.0f;
        float xSensibility = 1.0f;
        float ySensibility = 1.0f;

    public:
        explicit FreeCameraController() = default;
        FreeCameraController(const FreeCameraController&) = default;
        FreeCameraController(FreeCameraController&&) = default;

        FreeCameraController& operator=(const FreeCameraController&) = default;
        FreeCameraController& operator=(FreeCameraController&&) = default;

        void deserialise(const rapidjson::Value& src);
        rapidjson::Value serialise(rapidjson::Document& dest) const;

        /// Move camera with the given inputs. Takes the parameters for this controller into account
        void move(float strafe, float forward, float upward, float rotDx, float rotDy, double dt);

        void applyTo(const glm::vec2& viewportSize, Carrot::Camera& camera) const;
    };
}
