//
// Created by jglrxavpok on 31/12/2021.
//

#include "RigidBody.h"
#include <engine/utils/Macros.h>
#include <engine/physics/PhysicsSystem.h>
#include <engine/utils/conversions.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

namespace Carrot::Physics {
    RigidBody::BodyAccessWrite::BodyAccessWrite(const JPH::BodyID& bodyID) {
        if(!bodyID.IsInvalid()) {
            mutex = GetPhysics().lockWriteBody(bodyID);
            body = GetPhysics().lockedGetBody(bodyID);
            verify(body != nullptr, "Invalid bodyID");
        }
    }

    RigidBody::BodyAccessWrite::~BodyAccessWrite() {
        if(mutex) {
            GetPhysics().unlockWriteBody(mutex);
        }
    }

    JPH::Body* RigidBody::BodyAccessWrite::operator->() {
        return body;
    }

    RigidBody::BodyAccessWrite::operator bool() {
        return body != nullptr;
    }

    JPH::Body* RigidBody::BodyAccessWrite::get() {
        return body;
    }

    RigidBody::BodyAccessRead::BodyAccessRead(const JPH::BodyID& bodyID) {
        if(!bodyID.IsInvalid()) {
            mutex = GetPhysics().lockReadBody(bodyID);
            body = GetPhysics().lockedGetBody(bodyID);
            verify(body != nullptr, "Invalid bodyID");
        }
    }

    RigidBody::BodyAccessRead::~BodyAccessRead() {
        if(mutex) {
            GetPhysics().unlockReadBody(mutex);
        }
    }

    const JPH::Body* RigidBody::BodyAccessRead::operator->() {
        return body;
    }

    RigidBody::BodyAccessRead::operator bool() {
        return body != nullptr;
    }

    const JPH::Body* RigidBody::BodyAccessRead::get() {
        return body;
    }

    RigidBody::RigidBody() {
    }

    RigidBody::RigidBody(const RigidBody& toCopy): RigidBody() {
        *this = toCopy;
    }

    RigidBody::RigidBody(RigidBody&& toMove) {
        *this = std::move(toMove);
    }

    RigidBody& RigidBody::operator=(const RigidBody& toCopy) {
        verify(!toCopy.bodyID.IsInvalid(), "Used after std::move");

        if(&toCopy == this) {
            return *this;
        }

        destroyJoltRepresentation();

        bodyType = toCopy.bodyType;
        translationAxes = toCopy.translationAxes;
        rotationAxes = toCopy.rotationAxes;
        colliders.resize(toCopy.colliders.size());
        for (int i = 0; i < colliders.size(); ++i) {
            colliders[i] = std::make_unique<Collider>(toCopy.colliders[i]->getShape().duplicate(), toCopy.colliders[i]->getLocalTransform());
        }
        layerID = toCopy.layerID;

        JPH::BodyCreationSettings creationSettings;
        {
            BodyAccessRead toCopyBody{toCopy.bodyID};
            creationSettings = toCopyBody->GetBodyCreationSettings();
            bodyTemplate = toCopy.bodyTemplate;
        }
        createBody(creationSettings);

        return *this;
    }

    RigidBody& RigidBody::operator=(RigidBody&& toMove) {
        verify(!toMove.bodyID.IsInvalid(), "Used after std::move");
        bodyID = toMove.bodyID;

        dofConstraint = toMove.dofConstraint;
        toMove.dofConstraint = nullptr;

        bodyType = toMove.bodyType;
        bodyTemplate = std::move(toMove.bodyTemplate);
        BodyAccessWrite body{bodyID};
        if(body) {
            body->SetUserData((std::uint64_t)this);
        }

        colliders = std::move(toMove.colliders);
        translationAxes = toMove.translationAxes;
        rotationAxes = toMove.rotationAxes;
        layerID = toMove.layerID;

        toMove.bodyID = {};
        return *this;
    }

    void RigidBody::addColliderDirectly(std::unique_ptr<Collider>&& collider, std::size_t insertionIndex) {
        collider->addToBody(*this);

        if(insertionIndex < colliders.size()) {
            colliders.insert(colliders.begin() + insertionIndex, std::move(collider));
        } else {
            colliders.emplace_back(std::move(collider));
        }

        createBodyFromColliders();
    }

    Collider& RigidBody::addCollider(const CollisionShape& shape, const Carrot::Math::Transform& localTransform, std::size_t insertionIndex) {
        addColliderDirectly(std::make_unique<Collider>(shape.duplicate(), localTransform), insertionIndex);

        auto& collider = colliders.back();
        return *collider;
    }

    void RigidBody::removeCollider(std::size_t index) {
        verify(!bodyID.IsInvalid(), "Used after std::move");
        verify(index >= 0 && index < getColliderCount(), "Out of bounds");
        getCollider(index).removeFromBody(*this);
        colliders.erase(colliders.begin() + index);

        recreateBodyIfNeeded();
    }

    std::size_t RigidBody::getColliderCount() const {
        return colliders.size();
    }

    std::vector<std::unique_ptr<Collider>>& RigidBody::getColliders() {
        return colliders;
    }

    const std::vector<std::unique_ptr<Collider>>& RigidBody::getColliders() const {
        return colliders;
    }

    Collider& RigidBody::getCollider(std::size_t index) {
        return *colliders[index];
    }

    const Collider& RigidBody::getCollider(std::size_t index) const {
        return *colliders[index];
    }

    Carrot::Math::Transform RigidBody::getTransform() const {
        BodyAccessRead body{bodyID};
        Carrot::Math::Transform transform;
        transform.position = joltToCarrot(body->GetPosition());
        transform.rotation = joltToCarrot(body->GetRotation());
        return transform;
    }

    void RigidBody::setTransform(const Carrot::Math::Transform &transform) {
        bodyTemplate.mPosition = Carrot::carrotToJolt(transform.position);
        bodyTemplate.mRotation = Carrot::carrotToJolt(transform.rotation);
        recreateBodyIfNeeded();
    }

    static BodyType joltToCarrot(JPH::EMotionType motionType) {
        switch (motionType) {
            case JPH::EMotionType::Static:
                return BodyType::Static;
            case JPH::EMotionType::Kinematic:
                return BodyType::Kinematic;
            case JPH::EMotionType::Dynamic:
                return BodyType::Dynamic;

            default:
                TODO;
                return BodyType::Static;
        }
    }

    static JPH::EMotionType carrotToJolt(BodyType motionType) {
        switch (motionType) {
            case BodyType::Static:
                return JPH::EMotionType::Static;
            case BodyType::Kinematic:
                return JPH::EMotionType::Kinematic;
            case BodyType::Dynamic:
                return JPH::EMotionType::Dynamic;

            default:
                TODO;
                return JPH::EMotionType::Static;
        }
    }

    BodyType RigidBody::getBodyType() const {
        return bodyType;
    }

    void RigidBody::setBodyType(BodyType type) {
        BodyAccessWrite body{bodyID};
        bodyType = type;
        if(body) {
            body->SetMotionType(carrotToJolt(type));
        }
    }

    CollisionLayerID RigidBody::getCollisionLayer() const {
        return layerID;
    }

    void RigidBody::setCollisionLayer(CollisionLayerID id) {
        layerID = id;
        recreateBodyIfNeeded();
    }

    bool RigidBody::isActive() const {
        BodyAccessRead body{bodyID};
        if(!body) {
            return false;
        }
        return body->IsInBroadPhase();
    }

    void RigidBody::setActive(bool active) {
        if(active) {
            if(!isActive()) {
                GetPhysics().jolt->GetBodyInterface().AddBody(bodyID, JPH::EActivation::Activate);
            }
        } else {
            if(isActive()) {
                GetPhysics().jolt->GetBodyInterface().RemoveBody(bodyID);
            }
        }
    }

    float RigidBody::getMass() const {
        return bodyTemplate.mMassPropertiesOverride.mMass;
    }

    void RigidBody::setMass(float mass) {
        bodyTemplate.mMassPropertiesOverride.mMass = mass;
        BodyAccessWrite body{bodyID};
        if(body) {
            body->GetMotionProperties()->SetInverseMass(1.0f / mass);
        }
    }

    glm::vec3 RigidBody::getVelocity() const {
        BodyAccessRead body{bodyID};
        return Carrot::joltToCarrot(body->GetLinearVelocity());
    }

    void RigidBody::setVelocity(const glm::vec3& velocity) {
        BodyAccessWrite body{bodyID};
        if(body) {
            body->SetLinearVelocity(Carrot::carrotToJolt(velocity));
        } else {
            bodyTemplate.mLinearVelocity = Carrot::carrotToJolt(velocity);
        }
    }

    glm::bvec3 RigidBody::getTranslationAxes() const {
        return translationAxes;
    }

    void RigidBody::setTranslationAxes(const glm::bvec3& freeAxes) {
        translationAxes = freeAxes;
        setupDOFConstraint();
    }

    glm::bvec3 RigidBody::getRotationAxes() const {
        return rotationAxes;
    }

    void RigidBody::setRotationAxes(const glm::bvec3& freeAxes) {
        rotationAxes = freeAxes;
        setupDOFConstraint();
    }

    void RigidBody::setUserData(void* pData) {
        userData = pData;
    }

    void* RigidBody::getUserData() const {
        return userData;
    }

    void RigidBody::createBody(JPH::BodyCreationSettings creationSettings) {
        creationSettings.mUserData = (std::uint64_t)this;

        if(bodyType == BodyType::Static) {
            creationSettings.mObjectLayer = GetPhysics().getDefaultStaticLayer(); // TODO
        } else {
            const auto& layersManager = GetPhysics().getCollisionLayers();
            if(layersManager.isValid(layerID)) {
                creationSettings.mObjectLayer = GetPhysics().getCollisionLayers().getLayer(layerID).layerID;
            } else {
                creationSettings.mObjectLayer = GetPhysics().getDefaultMovingLayer();
            }
        }
        bodyID = GetPhysics().createRigidbody(creationSettings);
        setupDOFConstraint();
    }

    void RigidBody::recreateBodyIfNeeded() {
        if(bodyID.IsInvalid()) {
            return;
        }

        createBodyFromColliders();
    }

    void RigidBody::createBodyFromColliders() {
        verify(!colliders.empty(), "This body must have at least one collider");

        destroyJoltRepresentation();

        const JPH::Shape* bodyShape = nullptr;
        if(colliders.size() == 1) {
            bodyShape = colliders[0]->getShape().shape.GetPtr();
        } else {
            JPH::StaticCompoundShapeSettings compoundShapeSettings;
            for(auto& pCollider : colliders) {
                // local transform is already handled by collider
                compoundShapeSettings.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), pCollider->getShape().shape.GetPtr());
            }
            bodyShapeRef = compoundShapeSettings.Create().Get();
            bodyShape = bodyShapeRef.GetPtr();
        }

        JPH::BodyCreationSettings bodyCreationSettings = bodyTemplate;
        bodyCreationSettings.mMotionType = carrotToJolt(bodyType);
        bodyCreationSettings.SetShape(bodyShape);
        createBody(bodyCreationSettings);
    }

    void RigidBody::setupDOFConstraint() {
        if(dofConstraint) {
            GetPhysics().jolt->RemoveConstraint(dofConstraint);
            dofConstraint = nullptr;
        }

        if(bodyID.IsInvalid()) {
            return;
        }

        if(glm::all(rotationAxes) && glm::all(translationAxes)) {
            return;
        }

        BodyAccessWrite body{bodyID};
        JPH::SixDOFConstraintSettings settings;

        if(!rotationAxes.x)
            settings.MakeFixedAxis(JPH::SixDOFConstraintSettings::EAxis::RotationX);
        if(!rotationAxes.y)
            settings.MakeFixedAxis(JPH::SixDOFConstraintSettings::EAxis::RotationY);
        if(!rotationAxes.z)
            settings.MakeFixedAxis(JPH::SixDOFConstraintSettings::EAxis::RotationZ);

        if(!translationAxes.x)
            settings.MakeFixedAxis(JPH::SixDOFConstraintSettings::EAxis::TranslationX);
        if(!translationAxes.y)
            settings.MakeFixedAxis(JPH::SixDOFConstraintSettings::EAxis::TranslationY);
        if(!translationAxes.z)
            settings.MakeFixedAxis(JPH::SixDOFConstraintSettings::EAxis::TranslationZ);

        settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
        settings.mPosition1 = body->GetCenterOfMassPosition();

        dofConstraint = (JPH::SixDOFConstraint*)settings.Create(JPH::Body::sFixedToWorld, *body.get());
        GetPhysics().jolt->AddConstraint(dofConstraint);
    }

    void RigidBody::destroyJoltRepresentation() {
        if(dofConstraint) {
            GetPhysics().jolt->RemoveConstraint(dofConstraint);
            dofConstraint = nullptr;
        }
        if(!bodyID.IsInvalid()) {
            GetPhysics().destroyRigidbody(bodyID);
        }
        if(bodyShapeRef) {
            bodyShapeRef->Release();
            bodyShapeRef = {};
        }
    }

    RigidBody::~RigidBody() {
        destroyJoltRepresentation();
    }
}