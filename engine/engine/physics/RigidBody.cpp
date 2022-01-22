//
// Created by jglrxavpok on 31/12/2021.
//

#include "RigidBody.h"
#include <engine/utils/Macros.h>
#include <engine/physics/PhysicsSystem.h>
#include <engine/utils/conversions.h>

namespace Carrot::Physics {
    RigidBody::RigidBody() {
        body = GetPhysics().getPhysicsWorld().createRigidBody({});
        body->setUserData(this);
    }

    RigidBody::RigidBody(const RigidBody& toCopy): RigidBody() {
        *this = toCopy;
    }

    RigidBody::RigidBody(RigidBody&& toMove) {
        *this = std::move(toMove);
    }

    RigidBody& RigidBody::operator=(const RigidBody& toCopy) {
        verify(toCopy.body, "Used after std::move");

        if(&toCopy == this) {
            return *this;
        }

        if(body) {
            GetPhysics().getPhysicsWorld().destroyRigidBody(body);
        }

        body = GetPhysics().getPhysicsWorld().createRigidBody({});
        body->setUserData(this);

        body->setTransform(toCopy.body->getTransform());
        body->setMass(toCopy.body->getMass());
        body->setAngularDamping(toCopy.body->getAngularDamping());
        body->setAngularVelocity(toCopy.body->getAngularVelocity());
        body->setIsActive(toCopy.body->isActive());
        body->setIsAllowedToSleep(toCopy.body->isAllowedToSleep());
        body->setLinearDamping(toCopy.body->getLinearDamping());
        body->setLinearVelocity(toCopy.body->getLinearVelocity());
        body->setLocalCenterOfMass(toCopy.body->getLocalCenterOfMass());
        body->setLocalInertiaTensor(toCopy.body->getLocalInertiaTensor());
        body->setType(toCopy.body->getType());
        body->setUserData(this);

        for (std::size_t index = 0; index < toCopy.getColliderCount(); index++) {
            auto& colliderToCopy = toCopy.getCollider(index);
            auto& collider = addCollider(colliderToCopy.getShape(), colliderToCopy.getLocalTransform());
            /* TODO collider.setIsTrigger(colliderToCopy.getIsTrigger());
            collider.setCollideWithMaskBits(colliderToCopy.getCollideWithMaskBits());
            collider.setCollisionCategoryBits(colliderToCopy.getCollisionCategoryBits());
             */
        }
        return *this;
    }

    RigidBody& RigidBody::operator=(RigidBody&& toMove) {
        verify(toMove.body, "Used after std::move");
        body = toMove.body;
        body->setUserData(this);
        toMove.body = nullptr;
        return *this;
    }

    void RigidBody::addColliderDirectly(std::unique_ptr<Collider>&& collider, std::size_t insertionIndex) {
        collider->addToBody(*this);

        if(insertionIndex < colliders.size()) {
            colliders.insert(colliders.begin() + insertionIndex, std::move(collider));
        } else {
            colliders.emplace_back(std::move(collider));
        }
    }

    Collider& RigidBody::addCollider(const CollisionShape& shape, const Carrot::Math::Transform& localTransform, std::size_t insertionIndex) {
        verify(body, "Used after std::move");
        addColliderDirectly(std::make_unique<Collider>(shape.duplicate(), localTransform), insertionIndex);

        auto& collider = colliders.back();
        return *collider;
    }

    void RigidBody::removeCollider(std::size_t index) {
        verify(body, "Used after std::move");
        verify(index >= 0 && index < getColliderCount(), "Out of bounds");
        getCollider(index).removeFromBody(*this);
        colliders.erase(colliders.begin() + index);
    }

    std::size_t RigidBody::getColliderCount() const {
        verify(body, "Used after std::move");
        return body->getNbColliders();
    }

    std::vector<std::unique_ptr<Collider>>& RigidBody::getColliders() {
        verify(body, "Used after std::move");
        return colliders;
    }

    const std::vector<std::unique_ptr<Collider>>& RigidBody::getColliders() const {
        verify(body, "Used after std::move");
        return colliders;
    }

    Collider& RigidBody::getCollider(std::size_t index) {
        verify(body, "Used after std::move");
        return *colliders[index];
    }

    const Collider& RigidBody::getCollider(std::size_t index) const {
        verify(body, "Used after std::move");
        return *colliders[index];
    }

    Carrot::Math::Transform RigidBody::getTransform() const {
        verify(body, "Used after std::move");
        return body->getTransform();
    }

    void RigidBody::setTransform(const Carrot::Math::Transform &transform) {
        verify(body, "Used after std::move");
        body->setTransform(transform);
    }

    reactphysics3d::BodyType RigidBody::getBodyType() const {
        return body->getType();
    }

    void RigidBody::setBodyType(reactphysics3d::BodyType type) {
        body->setType(type);
    }

    bool RigidBody::isActive() const {
        return body->isActive();
    }

    void RigidBody::setActive(bool active) {
        body->setIsActive(active);
    }

    float RigidBody::getMass() const {
        return body->getMass();
    }

    void RigidBody::setMass(float mass) {
        body->setMass(mass);
    }

    glm::vec3 RigidBody::getLocalCenterOfMass() const {
        return Carrot::glmVecFromReactPhysics(body->getLocalCenterOfMass());
    }

    void RigidBody::setLocalCenterOfMass(const glm::vec3& center) {
        body->setLocalCenterOfMass(Carrot::reactPhysicsVecFromGlm(center));
    }

    glm::vec3 RigidBody::getLocalInertiaTensor() const {
        return Carrot::glmVecFromReactPhysics(body->getLocalInertiaTensor());
    }

    void RigidBody::setLocalInertiaTensor(const glm::vec3& inertia) {
        body->setLocalInertiaTensor(Carrot::reactPhysicsVecFromGlm(inertia));
    }

    RigidBody::~RigidBody() {
        if(body) {
            while (getColliderCount() > 0) {
                removeCollider(0);
            }
            GetPhysics().getPhysicsWorld().destroyRigidBody(body);
        }
    }
}