//
// Created by jglrxavpok on 06/08/2021.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <engine/utils/JSON.h>

namespace Carrot::ECS {
    struct Kinematics: public IdentifiableComponent<Kinematics> {
        glm::vec3 velocity{};

        explicit Kinematics(Entity entity): IdentifiableComponent<Kinematics>(std::move(entity)) {};

        explicit Kinematics(const rapidjson::Value& json, Entity entity): Kinematics(std::move(entity)) {
            velocity = JSON::read<3, float>(json["velocity"]);
        };

        rapidjson::Value toJSON(rapidjson::Document& doc) const override {
            return JSON::write(velocity, doc);
        }

        const char *const getName() const override {
            return "Kinematics";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<Kinematics>(newOwner);
            result->velocity = velocity;
            return result;
        }
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::Kinematics>::getStringRepresentation() {
    return "Kinematics";
}
