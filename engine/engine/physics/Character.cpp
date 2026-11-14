//
// Created by jglrxavpok on 29/06/2023.
//

#include "Character.h"
#include <engine/physics/PhysicsSystem.h>
#include <engine/utils/conversions.h>
#include <engine/utils/Macros.h>

namespace Carrot::Physics {
    static const float cCollisionTolerance = 0.05f;

    static Collider makeDefaultCollider() {
        auto capsuleShape = std::make_unique<CapsuleCollisionShape>(0.5f, 0.9f);

        Carrot::Math::Transform localTransform;
        localTransform.rotation = glm::rotate(glm::identity<glm::quat>(), glm::half_pi<float>(), glm::vec3{1,0,0});

        Collider collider{std::move(capsuleShape), localTransform};
        return collider;
    }

    Character::Character(PhysicsSystem& system): BaseBody(system), collider(makeDefaultCollider()) {
        characterSettings.mUp = JPH::Vec3::sAxisZ();
        characterSettings.mShape = collider.getShape().shape;
        characterSettings.mSupportingVolume = JPH::Plane { JPH::Vec3::sAxisZ(), -0.9f};
        characterSettings.mFriction = 0.2f;

        createJoltRepresentation();
    }

    Character::Character(const Character& other): BaseBody(other.getPhysicsSystem()), collider(other.collider.getShape().duplicate(), other.collider.getLocalTransform()) {
        *this = other;
    }

    Character::Character(Character&& other): BaseBody(other.getPhysicsSystem()), collider(std::move(other.collider)) {
        *this = std::move(other);
    }

    Character::~Character() {
        if(!physicsRepresentation) {
            return;
        }
        if(inWorld) {
            removeFromWorld();
        }
    }

    Character& Character::operator=(const Character& other) {
        BaseBody::operator=(other);
        collider = Collider{other.collider.getShape().duplicate(), other.collider.getLocalTransform()};
        characterSettings = other.characterSettings;
        worldTransform = other.worldTransform;
        velocity = other.velocity;
        dirtyTransform = other.dirtyTransform;
        onGround = other.onGround;
        layerID = other.layerID;

        createJoltRepresentation();

        if(other.inWorld) {
            addToWorld();
        }

        return *this;
    }

    Character& Character::operator=(Character&& other) {
        BaseBody::operator=(std::move(other));
        collider = std::move(other.collider);
        characterSettings = std::move(other.characterSettings);
        worldTransform = std::move(other.worldTransform);
        velocity = std::move(other.velocity);
        dirtyTransform = std::move(other.dirtyTransform);
        onGround = std::move(other.onGround);
        inWorld = other.inWorld;
        layerID = other.layerID;

        physicsRepresentation = std::move(other.physicsRepresentation);

        if(physicsRepresentation) {
            JPH::SharedMutex* bodyMutex = getPhysicsSystem().lockReadBody(physicsRepresentation->GetBodyID());
            CLEANUP(getPhysicsSystem().unlockReadBody(bodyMutex));
            JPH::Body* body = getPhysicsSystem().lockedGetBody(physicsRepresentation->GetBodyID());
            body->SetUserData((std::uint64_t)&bodyUserData);
        }
        return *this;
    }


    void Character::prePhysics() {
        if(dirtyTransform) {
            dirtyTransform = false;
            physicsRepresentation->SetPositionAndRotation(Carrot::carrotToJolt(worldTransform.position), Carrot::carrotToJolt(worldTransform.rotation), inWorld ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
        }

        if(dirtyVelocity && inWorld) {
            dirtyVelocity = false;
            physicsRepresentation->SetLinearVelocity(Carrot::carrotToJolt(velocity));
        }
    }

    void Character::postPhysics() {
        physicsRepresentation->PostSimulation(cCollisionTolerance);

        JPH::Vec3 position;
        JPH::Quat rotation;

        physicsRepresentation->GetPositionAndRotation(position, rotation);

        worldTransform.position = Carrot::joltToCarrot(position);
        worldTransform.rotation = Carrot::joltToCarrot(rotation);

        velocity = Carrot::joltToCarrot(physicsRepresentation->GetLinearVelocity());
        onGround = physicsRepresentation->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
    }

    Collider& Character::getCollider() {
        return collider;
    }

    const Collider& Character::getCollider() const {
        return collider;
    }

    void Character::setCollider(Collider&& newCollider) {
        collider = std::move(newCollider);
        createJoltRepresentation();
    }

    void Character::applyColliderChanges() {
        createJoltRepresentation();
    }

    void Character::setHeight(float height) {
        characterSettings.mSupportingVolume = JPH::Plane { JPH::Vec3::sAxisZ(), -height/2.0f };
        createJoltRepresentation();
    }

    void Character::setVelocity(const glm::vec3& velocity) {
        this->velocity = velocity;
        dirtyVelocity = true;
    }

    const glm::vec3& Character::getVelocity() const {
        return velocity;
    }

    void Character::setWorldTransform(const Carrot::Math::Transform& transform) {
        worldTransform = transform;
        dirtyTransform = true;
    }

    const Carrot::Math::Transform& Character::getWorldTransform() const {
        return worldTransform;
    }

    float Character::getMass() const {
        return characterSettings.mMass;
    }

    void Character::setMass(float mass) {
        characterSettings.mMass = mass;
        createJoltRepresentation();
    }

    bool Character::isOnGround() {
        return onGround;
    }

    void Character::jump(float speed) {
        TODO;
    }

    CollisionLayerID Character::getCollisionLayer() const {
        return layerID;
    }

    void Character::setCollisionLayer(CollisionLayerID id) {
        layerID = id;
        createJoltRepresentation();
    }

    bool Character::isInWorld() {
        return inWorld;
    }

    void Character::addToWorld() {
        if(!inWorld) {
            physicsRepresentation->AddToPhysicsSystem();
            inWorld = true;
        }
    }

    void Character::removeFromWorld() {
        if(inWorld) {
            physicsRepresentation->RemoveFromPhysicsSystem();
            inWorld = false;
        }
    }

    void Character::createJoltRepresentation() {
        bool wasInWorld = inWorld;
        removeFromWorld();

        const auto& layersManager = GetConfiguration().collisionLayers;
        if(layersManager.isValid(layerID)) {
            characterSettings.mLayer = GetConfiguration().collisionLayers.getLayer(layerID).layerID;
        } else {
            characterSettings.mLayer = GetConfiguration().collisionLayers.getMovingLayer();
        }

        characterSettings.mShape = collider.getShape().shape;
        physicsRepresentation = std::make_unique<JPH::Character>(&characterSettings,
                                                   Carrot::carrotToJolt(worldTransform.position),
                                                   Carrot::carrotToJolt(worldTransform.rotation),
                                                   (std::uint64_t)this,
                                                   getPhysicsSystem().jolt.get());
        {
            BodyAccessWrite body(getPhysicsSystem(), physicsRepresentation->GetBodyID());
            body->SetUserData((std::uint64_t)&bodyUserData);
        }
        if(wasInWorld) {
            addToWorld();
        }
    }
} // Carrot::Physics