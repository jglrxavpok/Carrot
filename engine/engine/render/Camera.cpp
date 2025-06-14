//
// Created by jglrxavpok on 06/12/2020.
//

#include "Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "engine/utils/Macros.h"

namespace Carrot {
    Camera::Camera(const glm::mat4& view, const glm::mat4& projection): type(ControlType::ViewProjection), viewMatrix(view), projectionMatrix(projection) {}

    Camera::Camera(float fov, float aspectRatio, float zNear, float zFar, glm::vec3 up): up(up), position(), target() {
        projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, zNear, zFar);
        projectionMatrix[1][1] *= -1; // convert to Vulkan coordinates (from OpenGL)
    }

    const glm::mat4& Camera::getProjectionMatrix() const {
        return projectionMatrix;
    }

    const glm::vec3& Camera::getUp() const {
        verify(type == ControlType::PoseAndLookAt, "Cannot get UP vector on non pose-and-look-at cameras!");
        return up;
    }

    const glm::mat4& Camera::getCurrentFrameViewMatrix() const {
        return viewMatrix;
    }

    const glm::mat4& Camera::getCurrentFrameProjectionMatrix() const {
        return projectionMatrix;
    }

    const glm::mat4& Camera::computeViewMatrix() {
        switch (type) {
            case ControlType::PoseAndLookAt:
                viewMatrix = glm::lookAt(position, target, up);
                return viewMatrix;

            case ControlType::ViewProjection:
                return viewMatrix;

            default:
                TODO;
        }
    }

    const glm::vec3& Camera::getTarget() const {
        verify(type == ControlType::PoseAndLookAt, "Cannot get TARGET vector on non pose-and-look-at cameras!");
        return target;
    }

    const glm::vec3& Camera::getPosition() const {
        verify(type == ControlType::PoseAndLookAt, "Cannot get POSITION vector on non pose-and-look-at cameras!");
        return position;
    }

    glm::vec3 Camera::computePosition() const {
        if (type == ControlType::PoseAndLookAt) {
            return position;
        } else if (type == ControlType::ViewProjection) {
            return viewMatrix[3];
        }
        TODO; // unhandled case
        return {};
    }

    glm::vec3& Camera::getTargetRef() {
        verify(type == ControlType::PoseAndLookAt, "Cannot get TARGET vector on non pose-and-look-at cameras!");
        return target;
    }

    glm::vec3& Camera::getPositionRef() {
        verify(type == ControlType::PoseAndLookAt, "Cannot get POSITION ref vector on non pose-and-look-at cameras!");
        return position;
    }

    glm::mat4& Camera::getViewMatrixRef() {
        verify(type == ControlType::ViewProjection, "Cannot get VIEW matrix on non view-projection cameras!");
        return viewMatrix;
    }

    glm::mat4& Camera::getProjectionMatrixRef() {
        return projectionMatrix;
    }

    void Camera::setTargetAndPosition(const glm::vec3& target, const glm::vec3& position) {
        type = ControlType::PoseAndLookAt;
        this->target = target;
        this->position = position;
    }

    void Camera::setViewProjection(const glm::mat4& view, const glm::mat4& projection) {
        type = ControlType::ViewProjection;
        this->viewMatrix = view;
        this->projectionMatrix = projection;
    }

    void Camera::updateFrustum() {
        glm::mat4 proj = projectionMatrix;
        //proj[1][1] *= -1;
        const glm::mat4 viewproj = proj * computeViewMatrix();

        // Based on
        // Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix
        // Gil Gribb
        // Klaus Hartmann
        // &
        // https://gist.github.com/podgorskiy/e698d18879588ada9014768e3e82a644

        constexpr int Left = 0;
        constexpr int Right = 1;
        constexpr int Bottom = 2;
        constexpr int Top = 3;
        constexpr int Near = 4;
        constexpr int Far = 5;
        glm::mat4 m = glm::transpose(viewproj);
        frustum[Left]   = m[3] + m[0];
        frustum[Right]  = m[3] - m[0];
        frustum[Bottom] = m[3] + m[1];
        frustum[Top]    = m[3] - m[1];
        frustum[Near]   = m[3] + m[2];
        frustum[Far]    = m[3] - m[2];

        for (auto& plane : frustum) {
            plane.normalize();
        }
    }

    const Math::Plane& Camera::getFrustumPlane(std::size_t index) const {
        return frustum[index];
    }

    bool Camera::isInFrustum(const Math::Sphere& sphere) const {
        for(int i = 0; i < 6; i++) {
            auto& plane = frustum[i];
            if(plane.getSignedDistance(sphere.center) < -sphere.radius) {
                return false;
            }
        }
        return true;
    }

    Camera& Camera::operator=(const Camera& toCopy) = default;
}