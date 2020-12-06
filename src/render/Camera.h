//
// Created by jglrxavpok on 06/12/2020.
//

#pragma once
#include "vulkan/includes.h"
#include "CameraBufferObject.h"

namespace Carrot {
    class Camera {
    private:
        glm::vec3 up{};
        glm::mat4 projectionMatrix{1.0f};

    public:
        glm::vec3 position{};
        glm::vec3 target{};

        explicit Camera(float fov, float aspectRatio, float zNear, float zFar, glm::vec3 up = {0,0,1});

        void updateBufferObject(CameraBufferObject& obj);
    };
}


