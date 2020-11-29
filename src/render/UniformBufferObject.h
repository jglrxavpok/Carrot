//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include "vulkan/includes.h"
#include <glm/glm.hpp>

namespace Carrot {
    /// UBO used by this engine
    struct UniformBufferObject {
        /// Projection matrix
        alignas(16) glm::mat4 projection;

        /// View matrix (transformation of the camera)
        alignas(16) glm::mat4 view;

        /// Model matrix (transformation of the model)
        alignas(16) glm::mat4 model;

        // TODO: split proj+view and model matrices into different UBO?
    };
}
