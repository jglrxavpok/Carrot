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
    };

    rapidjson::Value CharacterControllerComponent::toJSON(rapidjson::Document& doc) const {
        rapidjson::Value obj(rapidjson::kObjectType);

        obj.AddMember("headChildEntity", headChildName, doc.GetAllocator());
        obj.AddMember("yaw", yaw, doc.GetAllocator());
        obj.AddMember("pitch", pitch, doc.GetAllocator());

        return obj;
    }

    void CharacterControllerComponent::drawInspectorInternals(const Carrot::Render::Context& renderContext, bool& modified) {
        if(ImGui::InputText("Head child entity name##CharacterControllerComponent child name", headChildName, ImGuiInputTextFlags_EnterReturnsTrue)) {

        }
    }
}