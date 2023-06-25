//
// Created by jglrxavpok on 20/06/2023.
//

#pragma once

#include <glm/glm.hpp>

namespace Carrot::Physics {
    class RigidBody;
    class Collider;
    class CollisionShape;

    struct RaycastInfo {
        glm::vec3 worldPoint;
        glm::vec3 worldNormal;
        float t = -1.0f;
        Collider* collider = nullptr;
        RigidBody* rigidBody = nullptr;

    };

}