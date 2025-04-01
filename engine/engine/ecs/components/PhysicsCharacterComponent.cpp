#include <engine/utils/Macros.h>
#include <engine/Engine.h>
#include <engine/physics/PhysicsSystem.h>
#include "PhysicsCharacterComponent.h"

namespace Carrot::ECS {
    PhysicsCharacterComponent::PhysicsCharacterComponent(Carrot::ECS::Entity entity)
            : Carrot::ECS::IdentifiableComponent<PhysicsCharacterComponent>(std::move(entity)) {

    }

    PhysicsCharacterComponent::PhysicsCharacterComponent(const Carrot::DocumentElement& doc, Carrot::ECS::Entity entity)
            : PhysicsCharacterComponent(std::move(entity)) {
        if(doc.contains("mass")) {
            character.setMass(doc["mass"].getAsDouble());
        }
        if(doc.contains("collider")) {
            auto pCollider = Physics::Collider::load(doc["collider"]);
            character.setCollider(std::move(*pCollider));
        }
        if(doc.contains("layer")) {
            Physics::CollisionLayerID collisionLayer;
            if(!GetPhysics().getCollisionLayers().findByName(doc["layer"].getAsString(), collisionLayer)) {
                collisionLayer = GetPhysics().getDefaultMovingLayer();
            }
            character.setCollisionLayer(collisionLayer);
        }
        if (doc.contains("apply_rotation")) {
            applyRotation = doc["apply_rotation"].getAsBool();
        }
    }

    Carrot::DocumentElement PhysicsCharacterComponent::serialise() const {
        Carrot::DocumentElement obj;
        obj["mass"] = character.getMass();
        obj["collider"] = character.getCollider().serialise();
        obj["layer"] = GetPhysics().getCollisionLayers().getLayer(character.getCollisionLayer()).name;
        obj["apply_rotation"] = applyRotation;
        return obj;
    }

    std::unique_ptr <Carrot::ECS::Component> PhysicsCharacterComponent::duplicate(const Carrot::ECS::Entity& newOwner) const {
        auto result = std::make_unique<PhysicsCharacterComponent>(newOwner);
        result->character = character;
        result->applyRotation = applyRotation;
        return result;
    }

    void PhysicsCharacterComponent::dispatchEventsPostPhysicsMainThread() {
        character.dispatchEventsPostPhysicsMainThread();
    }

} // Carrot::ECS