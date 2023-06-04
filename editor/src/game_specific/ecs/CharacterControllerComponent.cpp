//
// Created by jglrxavpok on 24/03/2022.
//

#include "CharacterControllerComponent.h"
#include "core/utils/ImGuiUtils.hpp"

namespace Game::ECS {
    CharacterControllerComponent::CharacterControllerComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity): CharacterControllerComponent(std::move(entity)) {
        yaw = json["yaw"].GetFloat();
        pitch = json["pitch"].GetFloat();
        headChildName = json["headChildEntity"].GetString();
        scoreEntityName = json["scoreEntityName"].GetString();
        heldPages = json["heldPages"].GetUint();
    };

    rapidjson::Value CharacterControllerComponent::toJSON(rapidjson::Document& doc) const {
        rapidjson::Value obj(rapidjson::kObjectType);

        obj.AddMember("headChildEntity", headChildName, doc.GetAllocator());
        obj.AddMember("scoreEntityName", scoreEntityName, doc.GetAllocator());
        obj.AddMember("yaw", yaw, doc.GetAllocator());
        obj.AddMember("pitch", pitch, doc.GetAllocator());
        obj.AddMember("heldPages", heldPages, doc.GetAllocator());

        return obj;
    }

}