//
// Created by jglrxavpok on 29/06/2023.
//

#pragma once

#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Character/CharacterBase.h>
#include <Jolt/Physics/Character/Character.h>
#include <engine/physics/Colliders.h>
#include <glm/glm.hpp>

namespace Carrot::Physics {

    /**
     * Represents a physics body designed to be used for player and NPCs.
     * By default, has the shape of a capsule.
     */
    class Character {
    public:
        Character();
        Character(const Character&);
        Character(Character&&);
        ~Character();

        Character& operator=(const Character&);
        Character& operator=(Character&&);

    public:
        /// use 'applyColliderChanges' if you change this collider to make sure the character is aware of changes
        Collider& getCollider();

        const Collider& getCollider() const;

        /// update shape of character. applies change immediately
        void setCollider(Collider&& newCollider);

        void applyColliderChanges();

        /// updates height of the character. Used to determine a supporting plane, ie a plane
        /// which determines whether the character is on the ground or not
        void setHeight(float height);

        /// update velocity of character. applied next update
        void setVelocity(const glm::vec3& velocity);

        /// get current velocity of character, updated on update
        const glm::vec3& getVelocity() const;

        /// update transform of character. applied next update. scale is ignored
        void setWorldTransform(const Carrot::Math::Transform& transform);

        /// get world transform of character. updated on update, no scale is applied. updated by physics engine.
        const Carrot::Math::Transform& getWorldTransform() const;

        /// gets the mass of of the character
        float getMass() const;

        /// updates the mass of the character, triggers a recreation of the physics body
        void setMass(float mass);

        /// check if the character is on the ground
        bool isOnGround();

        /// applies an impulse along the Z axis
        void jump(float speed);

    public:
        void prePhysics();
        void postPhysics();

        void addToWorld();
        void removeFromWorld();

    private:
        void createJoltRepresentation();

    private:
        Carrot::Math::Transform worldTransform;
        glm::vec3 velocity{0.0f};
        bool dirtyTransform = false;
        bool dirtyVelocity = false;
        bool onGround = false;

        bool inWorld = false;

        Collider collider;
        JPH::CharacterSettings characterSettings;
        std::unique_ptr<JPH::Character> physics;
    };

} // Carrot::Physics
