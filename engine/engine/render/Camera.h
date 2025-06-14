//
// Created by jglrxavpok on 06/12/2020.
//

#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <core/math/Sphere.h>
#include <core/math/Plane.h>

namespace Carrot {
    /// To avoid update-order-dependent artefacts, the matrices used for rendering are only updated to the ones inside
    /// the camera when swapMatrices() is called
    class Camera {
    public:
        enum class ControlType {
            ViewProjection, // Full control
            PoseAndLookAt, // Set Position and Target members
        };

        explicit Camera(float fov, float aspectRatio, float zNear, float zFar, glm::vec3 up = {0,0,1});
        explicit Camera(const glm::mat4& view, const glm::mat4& projection);
        explicit Camera(): Camera(90.0f, 16.0f/9.0f, 0.0001f, 100.0f) {}

        Camera(const Camera& toCopy) {
            *this = toCopy;
        }

        const glm::mat4& computeViewMatrix();
    public:
        const glm::mat4& getProjectionMatrix() const;
        const glm::vec3& getUp() const;

        const glm::vec3& getTarget() const;
        const glm::vec3& getPosition() const;

        // Computes the world position of this camera, based on its viewMatrix.
        // For PoseAndLookAt positions, this is the same as calling getPosition, but with an additional copy
        glm::vec3 computePosition() const;

    public:
        void setTargetAndPosition(const glm::vec3& target, const glm::vec3& position);
        void setViewProjection(const glm::mat4& view, const glm::mat4& projection);

        /// Called after camera is modified by game, but before rendering
        void updateFrustum();

        const Math::Plane& getFrustumPlane(std::size_t index) const;

    public:
        bool isInFrustum(const Math::Sphere& sphere) const;

    public:
        glm::vec3& getTargetRef();
        glm::vec3& getPositionRef();

        glm::mat4& getViewMatrixRef();
        glm::mat4& getProjectionMatrixRef();

        const glm::mat4& getCurrentFrameViewMatrix() const;
        const glm::mat4& getCurrentFrameProjectionMatrix() const;

    public:
        [[nodiscard]] ControlType getControlMode() const { return type; }

    public:
        Camera& operator=(const Camera& toCopy);

    private:
        ControlType type = ControlType::PoseAndLookAt;
        glm::mat4 viewMatrix{1.0f};
        glm::mat4 projectionMatrix{1.0f};
        glm::vec3 up{};
        glm::vec3 position{};
        glm::vec3 target{};

        std::array<Math::Plane, 6> frustum;
    };
}


