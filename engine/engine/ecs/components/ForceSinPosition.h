//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <core/utils/JSON.h>
#include <imgui.h>

namespace Carrot::ECS {
    // TODO: still interesting??
    struct ForceSinPosition: public IdentifiableComponent<ForceSinPosition> {
        glm::vec3 angularFrequency{1.0f};
        glm::vec3 amplitude{1.0f};
        glm::vec3 angularOffset{0.0f};
        glm::vec3 centerPosition{0.0f};

        explicit ForceSinPosition(Entity entity): IdentifiableComponent<ForceSinPosition>(std::move(entity)) {};

        explicit ForceSinPosition(const rapidjson::Value& json, Entity entity): ForceSinPosition(std::move(entity)) {
            angularFrequency = JSON::read<3, float>(json["angularFrequency"]);
            amplitude = JSON::read<3, float>(json["amplitude"]);
            angularOffset = JSON::read<3, float>(json["angularOffset"]);
            centerPosition = JSON::read<3, float>(json["centerPosition"]);
        };

        rapidjson::Value toJSON(rapidjson::Document& doc) const override {
            rapidjson::Value obj(rapidjson::kObjectType);

            obj.AddMember("angularFrequency", JSON::write<3, float>(angularFrequency, doc), doc.GetAllocator());
            obj.AddMember("amplitude", JSON::write<3, float>(amplitude, doc), doc.GetAllocator());
            obj.AddMember("angularOffset", JSON::write<3, float>(angularOffset, doc), doc.GetAllocator());
            obj.AddMember("centerPosition", JSON::write<3, float>(centerPosition, doc), doc.GetAllocator());

            return obj;
        }

        const char *const getName() const override {
            return "ForceSinPosition";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<ForceSinPosition>(newOwner);
            result->angularFrequency = angularFrequency;
            result->amplitude = amplitude;
            result->angularOffset = angularOffset;
            result->centerPosition = centerPosition;
            return result;
        }
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::ForceSinPosition>::getStringRepresentation() {
    return "ForceSinPosition";
}