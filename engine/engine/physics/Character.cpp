//
// Created by jglrxavpok on 29/06/2023.
//

#include "Character.h"
#include <engine/physics/PhysicsSystem.h>
#include <engine/utils/conversions.h>

namespace Carrot::Physics {
    static const float cCollisionTolerance = 0.05f;

    Character::Character() {
        auto capsuleShape = std::make_unique<CapsuleCollisionShape>(0.5f, 0.9f);

        Carrot::Math::Transform localTransform;
        localTransform.rotation = glm::rotate(glm::identity<glm::quat>(), glm::half_pi<float>(), glm::vec3{1,0,0});

        Collider collider{std::move(capsuleShape), localTransform};
        characterSettings.mUp = JPH::Vec3::sAxisZ();
        characterSettings.mShape = collider.getShape().shape;
        characterSettings.mSupportingVolume = JPH::Plane { JPH::Vec3::sAxisZ(), -0.9f};
        characterSettings.mFriction = 0.5f;

        createJoltRepresentation();
    }

    Character::Character(const Character& other) {
        *this = other;
    }

    Character::Character(Character&& other) {
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
        characterSettings = other.characterSettings;
        worldTransform = other.worldTransform;
        velocity = other.velocity;
        dirtyTransform = other.dirtyTransform;
        onGround = other.onGround;

        createJoltRepresentation();

        if(other.inWorld) {
            addToWorld();
        }

        return *this;
    }

    Character& Character::operator=(Character&& other) {
        characterSettings = std::move(other.characterSettings);
        worldTransform = std::move(other.worldTransform);
        velocity = std::move(other.velocity);
        dirtyTransform = std::move(other.dirtyTransform);
        onGround = std::move(other.onGround);
        inWorld = other.inWorld;

        physics = std::move(other.physics);
        return *this;
    }


    void Character::update(double deltaTime) {
        physics->PostSimulation(cCollisionTolerance);

        if(dirtyTransform) {
            dirtyTransform = false;
            physics->SetPositionAndRotation(Carrot::carrotToJolt(worldTransform.position), Carrot::carrotToJolt(worldTransform.rotation));
        } else {
            JPH::Vec3 position;
            JPH::Quat rotation;

            physics->GetPositionAndRotation(position, rotation);

            worldTransform.position = Carrot::joltToCarrot(position);
            worldTransform.rotation = Carrot::joltToCarrot(rotation);
        }

        if(dirtyVelocity) {
            physics->SetLinearVelocity(Carrot::carrotToJolt(velocity));
            dirtyVelocity = false;
        }
        velocity = Carrot::joltToCarrot(physics->GetLinearVelocity());

        onGround = physics->IsSupported();
    }

    void Character::setShape(const CollisionShape& shape) {
        physics->SetShape(shape.shape.GetPtr(), cCollisionTolerance);
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

    bool Character::isOnGround() {
        return onGround;
    }

    void Character::jump(float speed) {
        TODO;
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
        characterSettings.mLayer = Carrot::Physics::Layers::MOVING; // TODO
        physics = std::make_unique<JPH::Character>(&characterSettings,
                                                   Carrot::carrotToJolt(worldTransform.position),
                                                   Carrot::carrotToJolt(worldTransform.rotation),
                                                   (std::uint64_t)this,
                                                   GetPhysics().jolt.get());
        inWorld = false;
    }
} // Carrot::Physics