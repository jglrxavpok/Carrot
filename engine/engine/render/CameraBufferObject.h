//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include "Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Carrot {
    namespace Render {
        struct Context;
    }

    /// UBO used by this engine
    struct CameraBufferObject {
        /// View matrix (transformation of the camera)
        alignas(16) glm::mat4 view;

        /// Inverse view matrix (transformation of the camera)
        alignas(16) glm::mat4 inverseView;

        /// Projection matrix
        alignas(16) glm::mat4 jitteredProjection;

        /// Use for motion vectors and things that should not move from one frame to the next
        alignas(16) glm::mat4 nonJitteredProjection;

        /// Inverse view matrix (transformation of the camera)
        alignas(16) glm::mat4 inverseJitteredProjection;

        /// Use for motion vectors and things that should not move from one frame to the next
        alignas(16) glm::mat4 inverseNonJitteredProjection;

        /// Frustum planes of camera (in world space)
        alignas(16) std::array<Carrot::Math::Plane, 6> frustum;

        void update(Camera& camera, const Render::Context& renderContext);
    };
}
