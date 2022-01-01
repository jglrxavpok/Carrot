//
// Created by jglrxavpok on 31/12/2021.
//

#include "RigidBody.h"
#include <engine/utils/Macros.h>
#include <engine/physics/PhysicsSystem.h>

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
        return *this;
    }

    RigidBody& RigidBody::operator=(RigidBody&& toMove) {
        verify(toMove.body, "Used after std::move");
        body = toMove.body;
        body->setUserData(this);
        toMove.body = nullptr;
        return *this;
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

    RigidBody::~RigidBody() {
        if(body) {
            GetPhysics().getPhysicsWorld().destroyRigidBody(body);
        }
    }
}