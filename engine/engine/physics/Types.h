//
// Created by jglrxavpok on 20/06/2023.
//

#pragma once

#include <glm/glm.hpp>
#include <Jolt/Physics/Collision/ObjectLayer.h>

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

    using CollisionLayerID = JPH::ObjectLayer;

    /**
    * Collision layers are used to determine which bodies are allowed to collide with one another.
    */
    struct CollisionLayer {
        CollisionLayerID layerID;
        std::string name;
        bool isStatic = false;
        bool isValid = false;
    };

}