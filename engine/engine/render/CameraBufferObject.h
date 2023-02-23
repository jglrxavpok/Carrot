//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include "Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Carrot {
    /// UBO used by this engine
    struct CameraBufferObject {
        /// Projection matrix
        alignas(16) glm::mat4 projection;

        /// View matrix (transformation of the camera)
        alignas(16) glm::mat4 view;

        /// Inverse view matrix (transformation of the camera)
        alignas(16) glm::mat4 inverseView;

        /// Inverse view matrix (transformation of the camera)
        alignas(16) glm::mat4 inverseProjection;

        void update(Camera& camera) {
            view = camera.getCurrentFrameViewMatrix();
            inverseView = glm::inverse(view);
            projection = camera.getCurrentFrameProjectionMatrix();
            inverseProjection = glm::inverse(projection);
        };
    };
}
