//
// Created by jglrxavpok on 31/12/2021.
//

#include "RigidBodyComponent.h"
#include "engine/utils/Macros.h"
#include "engine/physics/PhysicsSystem.h"
#include <core/utils/JSON.h>
#include <core/io/Logging.hpp>
#include <engine/render/Model.h>
#include <engine/Engine.h>
#include <engine/edition/DragDropTypes.h>
#include <engine/utils/conversions.h>
#include <core/utils/ImGuiUtils.hpp>
#include <imgui.h>
#include <map>
#include <filesystem>
#include <core/io/DocumentHelpers.h>
#include <engine/scripting/CSharpBindings.h>

namespace Carrot::ECS {

    RigidBodyComponent::RigidBodyComponent(Entity entity): IdentifiableComponent<RigidBodyComponent>(std::move(entity)) {
    }

    RigidBodyComponent::RigidBodyComponent(const Carrot::DocumentElement& doc, Entity entity): RigidBodyComponent(std::move(entity)) {
        rigidbody.setBodyType(getTypeFromName(std::string{doc["body_type"].getAsString()}));
        rigidbody.setMass(doc["mass"].getAsDouble());

        if (doc.contains("sensor")) {
            rigidbody.setSensor(doc["sensor"].getAsBool());
        }

        if(doc.contains("free_translation_axes")) {
            rigidbody.setTranslationAxes(Carrot::DocumentHelpers::read<3, bool>(doc["free_translation_axes"]));
        }
        if(doc.contains("free_rotation_axes")) {
            rigidbody.setRotationAxes(Carrot::DocumentHelpers::read<3, bool>(doc["free_rotation_axes"]));
        }
        if(doc.contains("layer")) {
            Physics::CollisionLayerID collisionLayer;
            if(!GetPhysics().getCollisionLayers().findByName(doc["layer"].getAsString(), collisionLayer)) {
                collisionLayer = GetPhysics().getDefaultMovingLayer();
            }
            rigidbody.setCollisionLayer(collisionLayer);
        }
        if (doc.contains("friction")) {
            rigidbody.setFriction(doc["friction"].getAsDouble());
        }
        if (doc.contains("linear_damping")) {
            rigidbody.setLinearDamping(doc["linear_damping"].getAsDouble());
        }

        for(const auto& colliderData : doc["colliders"].getAsArray()) {
            rigidbody.addColliderDirectly(Physics::Collider::load(colliderData));
        }

        loadCallbackHandle = GetCSharpBindings().registerGameAssemblyLoadCallback([&]() { callbacksHolder = nullptr; });
        unloadCallbackHandle = GetCSharpBindings().registerGameAssemblyUnloadCallback([&]() { callbacksHolder = nullptr; });
    }

    Carrot::DocumentElement RigidBodyComponent::serialise() const {
        Carrot::DocumentElement obj;
        Carrot::DocumentElement bodyType;
        bodyType = getTypeName(rigidbody.getBodyType());
        obj["body_type"] = bodyType;
        obj["mass"] = rigidbody.getMass();
        obj["sensor"] = rigidbody.isSensor();
        obj["free_translation_axes"] = Carrot::DocumentHelpers::write(rigidbody.getTranslationAxes());
        obj["free_rotation_axes"] = Carrot::DocumentHelpers::write(rigidbody.getRotationAxes());
        obj["layer"] = GetPhysics().getCollisionLayers().getLayer(rigidbody.getCollisionLayer()).name;
        obj["friction"] = rigidbody.getFriction();
        obj["linear_damping"] = rigidbody.getLinearDamping();

        Carrot::DocumentElement colliders{ DocumentType::Array };
        for(const auto& collider : rigidbody.getColliders()) {
            colliders.pushBack() = collider->serialise();
        }

        obj["colliders"] = colliders;
        return obj;
    }

    const char* RigidBodyComponent::getTypeName(const Physics::BodyType& type) {
        switch (type) {
            case Physics::BodyType::Dynamic:
                return "Dynamic";

            case Physics::BodyType::Kinematic:
                return "Kinematic";

            case Physics::BodyType::Static:
                return "Static";
        }
        verify(false, "missing case");
        return "Dynamic";
    }

    Physics::BodyType RigidBodyComponent::getTypeFromName(const std::string& name) {
        if(stricmp(name.c_str(), "Dynamic") == 0) {
            return Physics::BodyType::Dynamic;
        }
        if(stricmp(name.c_str(), "Kinematic") == 0) {
            return Physics::BodyType::Kinematic;
        }
        if(stricmp(name.c_str(), "Static") == 0) {
            return Physics::BodyType::Static;
        }
        verify(false, "invalid string");
        return Physics::BodyType::Dynamic;
    }

    void RigidBodyComponent::reload() {
        rigidbody.setActive(wasActive);
    }

    void RigidBodyComponent::unload() {
        wasActive = rigidbody.isActive();
        rigidbody.setActive(false);
    }

    void RigidBodyComponent::dispatchEventsPostPhysicsMainThread() {
        rigidbody.dispatchEventsPostPhysicsMainThread();
    }

    bool& RigidBodyComponent::getRegisteredForContactsRef() {
        return registeredForContacts;
    }

    std::shared_ptr<Scripting::CSObject>& RigidBodyComponent::getCallbacksHolder() {
        return callbacksHolder;
    }

}
