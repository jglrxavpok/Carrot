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

namespace Carrot::ECS {

    RigidBodyComponent::RigidBodyComponent(const rapidjson::Value& json, Entity entity): RigidBodyComponent(std::move(entity)) {
        rigidbody.setBodyType(getTypeFromName(json["body_type"].GetString()));
        rigidbody.setMass(json["mass"].GetFloat());
        rigidbody.setLocalInertiaTensor(Carrot::JSON::read<3, float>(json["local_inertia_tensor"]));
        rigidbody.setLocalCenterOfMass(Carrot::JSON::read<3, float>(json["local_center_of_mass"]));

        if(json.HasMember("free_translation_axes")) {
            rigidbody.setTranslationAxes(Carrot::JSON::read<3, float>(json["free_translation_axes"]));
        }
        if(json.HasMember("free_rotation_axes")) {
            rigidbody.setRotationAxes(Carrot::JSON::read<3, float>(json["free_rotation_axes"]));
        }

        for(const auto& colliderData : json["colliders"].GetArray()) {
            rigidbody.addColliderDirectly(Physics::Collider::loadFromJSON(colliderData));
        }
    }

    rapidjson::Value RigidBodyComponent::toJSON(rapidjson::Document& doc) const {
        rapidjson::Value obj{ rapidjson::kObjectType };
        rapidjson::Value bodyType { getTypeName(rigidbody.getBodyType()), doc.GetAllocator() };
        obj.AddMember("body_type", bodyType, doc.GetAllocator());
        obj.AddMember("mass", rigidbody.getMass(), doc.GetAllocator());
        obj.AddMember("local_inertia_tensor", Carrot::JSON::write(rigidbody.getLocalInertiaTensor(), doc), doc.GetAllocator());
        obj.AddMember("local_center_of_mass", Carrot::JSON::write(rigidbody.getLocalCenterOfMass(), doc), doc.GetAllocator());
        obj.AddMember("free_translation_axes", Carrot::JSON::write(rigidbody.getTranslationAxes(), doc), doc.GetAllocator());
        obj.AddMember("free_rotation_axes", Carrot::JSON::write(rigidbody.getRotationAxes(), doc), doc.GetAllocator());

        rapidjson::Value colliders{ rapidjson::kArrayType };
        for(const auto& collider : rigidbody.getColliders()) {
            colliders.PushBack(collider->toJSON(doc.GetAllocator()), doc.GetAllocator());
        }

        obj.AddMember("colliders", colliders, doc.GetAllocator());
        return obj;
    }

    const char* RigidBodyComponent::getTypeName(reactphysics3d::BodyType type) {
        switch (type) {
            case reactphysics3d::BodyType::DYNAMIC:
                return "Dynamic";

            case reactphysics3d::BodyType::KINEMATIC:
                return "Kinematic";

            case reactphysics3d::BodyType::STATIC:
                return "Static";
        }
        verify(false, "missing case");
        return "Dynamic";
    }

    const char* RigidBodyComponent::getShapeName(reactphysics3d::CollisionShapeType type) {
        switch (type) {
            case reactphysics3d::CollisionShapeType::SPHERE:
                return "Sphere Collider";

            case reactphysics3d::CollisionShapeType::CAPSULE:
                return "Capsule Collider";

            case reactphysics3d::CollisionShapeType::CONCAVE_SHAPE:
                return "Concave Shape Collider";

            case reactphysics3d::CollisionShapeType::CONVEX_POLYHEDRON:
                return "Mesh Collider";
        }
        verify(false, "missing case");
        return "Sphere Collider";
    }

    reactphysics3d::BodyType RigidBodyComponent::getTypeFromName(const std::string& name) {
        if(_stricmp(name.c_str(), "Dynamic") == 0) {
            return reactphysics3d::BodyType::DYNAMIC;
        }
        if(_stricmp(name.c_str(), "Kinematic") == 0) {
            return reactphysics3d::BodyType::KINEMATIC;
        }
        if(_stricmp(name.c_str(), "Static") == 0) {
            return reactphysics3d::BodyType::STATIC;
        }
        verify(false, "invalid string");
        return reactphysics3d::BodyType::DYNAMIC;
    }

    void RigidBodyComponent::reload() {
        rigidbody.setActive(wasActive);
    }

    void RigidBodyComponent::unload() {
        wasActive = rigidbody.isActive();
        rigidbody.setActive(false);
    }

}