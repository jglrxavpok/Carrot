//
// Created by jglrxavpok on 31/12/2021.
//

#pragma once

#include <reactphysics3d/reactphysics3d.h>
#include <engine/math/Transform.h>
#include "Colliders.h"

namespace Carrot::ECS {
    class RigidBodyComponent;
}

namespace Carrot::Physics {
    class RigidBody {
    public:
        explicit RigidBody();

        RigidBody(const RigidBody& toCopy);
        RigidBody(RigidBody&& toMove);

        RigidBody& operator=(const RigidBody& toCopy);
        RigidBody& operator=(RigidBody&& toMove);

        ~RigidBody();

    public:
        Collider& addCollider(const CollisionShape& shape, const Carrot::Math::Transform& localTransform = {});
        void removeCollider(std::size_t index);

        std::size_t getColliderCount() const;
        std::vector<std::unique_ptr<Collider>>& getColliders();
        const std::vector<std::unique_ptr<Collider>>& getColliders() const;

        Collider& getCollider(std::size_t index);
        const Collider& getCollider(std::size_t index) const;

    public:
        reactphysics3d::BodyType getBodyType() const;
        void setBodyType(reactphysics3d::BodyType type);

    public:
        float getMass() const;
        void setMass(float mass);

        glm::vec3 getLocalCenterOfMass() const;
        void setLocalCenterOfMass(const glm::vec3& center);

        glm::vec3 getLocalInertiaTensor() const;
        void setLocalInertiaTensor(const glm::vec3& inertia);

    public:
        bool isActive() const;
        void setActive(bool active);

    public:
        Carrot::Math::Transform getTransform() const;
        /// Forces the given transform. Only use for static bodies, kinematic and dynamic bodies may have trouble adapting to the new transform
        void setTransform(const Carrot::Math::Transform& transform);

        void addColliderDirectly(std::unique_ptr<Collider>&& collider);

    private:
        rp3d::RigidBody* body = nullptr;
        std::vector<std::unique_ptr<Collider>> colliders;

        friend class Collider;
    };
}
