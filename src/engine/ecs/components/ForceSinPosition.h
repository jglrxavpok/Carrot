//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Carrot::ECS {
    struct ForceSinPosition: public IdentifiableComponent<ForceSinPosition> {
        glm::vec3 angularFrequency{1.0f};
        glm::vec3 amplitude{1.0f};
        glm::vec3 angularOffset{0.0f};
        glm::vec3 centerPosition{0.0f};

        explicit ForceSinPosition(Entity entity): IdentifiableComponent<ForceSinPosition>(std::move(entity)) {};

        explicit ForceSinPosition(const rapidjson::Value& json, Entity entity): ForceSinPosition(std::move(entity)) {
            auto angularFrequencyArr = json["angularFrequency"].GetArray();
            auto amplitudeArr = json["amplitude"].GetArray();
            auto angularOffsetArr = json["angularOffset"].GetArray();
            auto centerPositionArr = json["centerPosition"].GetArray();

            angularFrequency = glm::vec3 { angularFrequencyArr[0].GetFloat(), angularFrequencyArr[1].GetFloat(), angularFrequencyArr[2].GetFloat() };
        };

        rapidjson::Value toJSON(rapidjson::Document& doc) const override {
            rapidjson::Value arr(rapidjson::kArrayType);

            TODO

            return arr;
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