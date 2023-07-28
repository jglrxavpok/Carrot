//
// Created by jglrxavpok on 29/06/2023.
//

#include "Character.h"
#include <engine/physics/PhysicsSystem.h>
#include <engine/utils/conversions.h>

namespace Carrot::Physics {
    static const float cCollisionTolerance = 0.05f;

    static Collider makeDefaultCollider() {
        auto capsuleShape = std::make_unique<CapsuleCollisionShape>(0.5f, 0.9f);

        Carrot::Math::Transform localTransform;
        localTransform.rotation = glm::rotate(glm::identity<glm::quat>(), glm::half_pi<float>(), glm::vec3{1,0,0});

        Collider collider{std::move(capsuleShape), localTransform};
        return collider;
    }

    Character::Character(): collider(makeDefaultCollider()) {
        characterSettings.mUp = JPH::Vec3::sAxisZ();
        characterSettings.mShape = collider.getShape().shape;
        characterSettings.mSupportingVolume = JPH::Plane { JPH::Vec3::sAxisZ(), -0.9f};
        characterSettings.mFriction = 0.2f;

        createJoltRepresentation();
    }

    Character::Character(const Character& other): collider(other.collider.getShape().duplicate(), other.collider.getLocalTransform()) {
        *this = other;
    }

    Character::Character(Character&& other): collider(std::move(other.collider)) {
        *this = std::move(other);
    }

    Character::~Character() {
        if(!physics) {
            return;
        }
        if(inWorld) {
            removeFromWorld();
        }
    }

    Character& Character::operator=(const Character& other) {
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
        collider = std::move(other.collider);
        characterSettings = std::move(other.characterSettings);
        worldTransform = std::move(other.worldTransform);
        velocity = std::move(other.velocity);
        dirtyTransform = std::move(other.dirtyTransform);
        onGround = std::move(other.onGround);
        inWorld = other.inWorld;
        layerID = other.layerID;

        physics = std::move(other.physics);
        return *this;
    }


    void Character::prePhysics() {
        if(dirtyTransform) {
            dirtyTransform = false;
            physics->SetPositionAndRotation(Carrot::carrotToJolt(worldTransform.position), Carrot::carrotToJolt(worldTransform.rotation), inWorld ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
        }

        if(dirtyVelocity && inWorld) {
            dirtyVelocity = false;
            physics->SetLinearVelocity(Carrot::carrotToJolt(velocity));
        }
    }

    void Character::postPhysics() {
        physics->PostSimulation(cCollisionTolerance);

        JPH::Vec3 position;
        JPH::Quat rotation;

        physics->GetPositionAndRotation(position, rotation);

        worldTransform.position = Carrot::joltToCarrot(position);
        worldTransform.rotation = Carrot::joltToCarrot(rotation);

        velocity = Carrot::joltToCarrot(physics->GetLinearVelocity());
        onGround = physics->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
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
            physics->AddToPhysicsSystem();
            inWorld = true;
        }
    }

    void Character::removeFromWorld() {
        if(inWorld) {
            physics->RemoveFromPhysicsSystem();
            inWorld = false;
        }
    }

    void Character::createJoltRepresentation() {
        bool wasInWorld = inWorld;
        removeFromWorld();

        const auto& layersManager = GetPhysics().getCollisionLayers();
        if(layersManager.isValid(layerID)) {
            characterSettings.mLayer = GetPhysics().getCollisionLayers().getLayer(layerID).layerID;
        } else {
            characterSettings.mLayer = GetPhysics().getDefaultMovingLayer();
        }

        characterSettings.mShape = collider.getShape().shape;
        physics = std::make_unique<JPH::Character>(&characterSettings,
                                                   Carrot::carrotToJolt(worldTransform.position),
                                                   Carrot::carrotToJolt(worldTransform.rotation),
                                                   (std::uint64_t)this,
                                                   GetPhysics().jolt.get());
        if(wasInWorld) {
            addToWorld();
        }
    }
} // Carrot::Physics