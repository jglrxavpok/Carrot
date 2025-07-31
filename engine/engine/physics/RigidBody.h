//
// Created by jglrxavpok on 31/12/2021.
//

#pragma once

#include <Jolt/Core/Mutex.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/Body.h>
#include <engine/math/Transform.h>
#include "Colliders.h"
#include "BodyUserData.h"
#include "core/async/Locks.h"
#include "eventpp/callbacklist.h"
#include <engine/physics/PhysicsBody.h>

namespace Carrot::ECS {
    class RigidBodyComponent;
}

namespace Carrot::Physics {
    enum class BodyType {
        Static,
        Dynamic,
        Kinematic,
    };

    class RigidBody: public BaseBody {
    public:
        explicit RigidBody();

        RigidBody(const RigidBody& toCopy);
        RigidBody(RigidBody&& toMove);

        RigidBody& operator=(const RigidBody& toCopy);
        RigidBody& operator=(RigidBody&& toMove);

        ~RigidBody();

    public:
        ///
        /// \param shape
        /// \param localTransform
        /// \param insertionIndex used to add a collider at a given index. Used to preserve order when removing and re-adding a collider (sometimes required for MeshCollisionShapes-based colliders)
        /// \return
        Collider& addCollider(const CollisionShape& shape, const Carrot::Math::Transform& localTransform = {}, std::size_t insertionIndex = std::numeric_limits<std::size_t>::max());
        void removeCollider(std::size_t index);

        /// Removes the given collider and reattaches it. Required for some CollisionShapes that need a full "reload" when changed.
        void reattach(std::size_t index);

        std::size_t getColliderCount() const;
        std::vector<std::unique_ptr<Collider>>& getColliders();
        const std::vector<std::unique_ptr<Collider>>& getColliders() const;

        Collider& getCollider(std::size_t index);
        const Collider& getCollider(std::size_t index) const;

    public:
        glm::vec3 getPointVelocity(const glm::vec3& point);
        void addForceAtPoint(const glm::vec3& force, const glm::vec3& point);
        void addRelativeForce(const glm::vec3& force);

    public:
        BodyType getBodyType() const;
        void setBodyType(BodyType type);

        CollisionLayerID getCollisionLayer() const;
        void setCollisionLayer(CollisionLayerID id);

    public:
        float getMass() const;
        void setMass(float mass);

        float getFriction() const;
        void setFriction(float newValue);

        float getLinearDamping() const;
        void setLinearDamping(float newValue);

        glm::vec3 getAngularVelocityInLocalSpace() const;
        glm::vec3 getVelocity() const;
        void setVelocity(const glm::vec3& velocity);

    public: // axes on which translation & rotation are allowed
        glm::bvec3 getTranslationAxes() const;
        void setTranslationAxes(const glm::bvec3& freeAxes);

        glm::bvec3 getRotationAxes() const;
        void setRotationAxes(const glm::bvec3& freeAxes);

    public:
        bool isActive() const;
        void setActive(bool active);

    public:
        Carrot::Math::Transform getTransform() const;
        /// Forces the given transform. Only use for static bodies, kinematic and dynamic bodies may have trouble adapting to the new transform
        void setTransform(const Carrot::Math::Transform& transform);

        /// \param insertionIndex used to add a collider at a given index. Used to preserve order when removing and re-adding a collider (sometimes required for MeshCollisionShapes-based colliders)
        void addColliderDirectly(std::unique_ptr<Collider>&& collider, std::size_t insertionIndex = std::numeric_limits<std::size_t>::max());

    public: // user-specified data (not forwarded to JoltPhysics, JoltPhysics bodies will have this instance as user-data)
        void* getUserData() const;
        void setUserData(void* pData);

    private:
        void createBody(JPH::BodyCreationSettings creationSettings);
        void createBodyFromColliders();
        void recreateBodyIfNeeded();

        void setupDOFConstraint();
        void destroyJoltRepresentation();

        std::vector<std::unique_ptr<Collider>> colliders;
        BodyType bodyType = BodyType::Dynamic;
        CollisionLayerID layerID = 1/*default moving layer*/;
        glm::bvec3 translationAxes{true, true, true};
        glm::bvec3 rotationAxes{true, true, true};

        JPH::BodyCreationSettings bodyTemplate;

        JPH::BodyID bodyID;
        JPH::ShapeRefC bodyShapeRef; // compound shape used when there are multiple colliders
        JPH::SixDOFConstraint* dofConstraint = nullptr;
        BodyUserData bodyUserData { BodyUserData::Type::Rigidbody, this };
        void* userData = nullptr;

        friend class Collider;
    };
}
