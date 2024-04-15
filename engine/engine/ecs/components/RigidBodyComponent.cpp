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

        if(json.HasMember("free_translation_axes")) {
            rigidbody.setTranslationAxes(Carrot::JSON::read<3, bool>(json["free_translation_axes"]));
        }
        if(json.HasMember("free_rotation_axes")) {
            rigidbody.setRotationAxes(Carrot::JSON::read<3, bool>(json["free_rotation_axes"]));
        }
        if(json.HasMember("layer")) {
            Physics::CollisionLayerID collisionLayer;
            if(!GetPhysics().getCollisionLayers().findByName(std::string_view { json["layer"].GetString(), json["layer"].GetStringLength() }, collisionLayer)) {
                collisionLayer = GetPhysics().getDefaultMovingLayer();
            }
            rigidbody.setCollisionLayer(collisionLayer);
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
        obj.AddMember("free_translation_axes", Carrot::JSON::write(rigidbody.getTranslationAxes(), doc), doc.GetAllocator());
        obj.AddMember("free_rotation_axes", Carrot::JSON::write(rigidbody.getRotationAxes(), doc), doc.GetAllocator());
        obj.AddMember("layer", rapidjson::Value(GetPhysics().getCollisionLayers().getLayer(rigidbody.getCollisionLayer()).name.c_str(), doc.GetAllocator()), doc.GetAllocator());

        rapidjson::Value colliders{ rapidjson::kArrayType };
        for(const auto& collider : rigidbody.getColliders()) {
            colliders.PushBack(collider->toJSON(doc.GetAllocator()), doc.GetAllocator());
        }

        obj.AddMember("colliders", colliders, doc.GetAllocator());
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
        if(_stricmp(name.c_str(), "Dynamic") == 0) {
            return Physics::BodyType::Dynamic;
        }
        if(_stricmp(name.c_str(), "Kinematic") == 0) {
            return Physics::BodyType::Kinematic;
        }
        if(_stricmp(name.c_str(), "Static") == 0) {
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

}