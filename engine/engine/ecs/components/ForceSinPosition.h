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
#include <core/io/DocumentHelpers.h>

namespace Carrot::ECS {
    // TODO: still interesting??
    struct ForceSinPosition: public IdentifiableComponent<ForceSinPosition> {
        glm::vec3 angularFrequency{1.0f};
        glm::vec3 amplitude{1.0f};
        glm::vec3 angularOffset{0.0f};
        glm::vec3 centerPosition{0.0f};

        explicit ForceSinPosition(Entity entity): IdentifiableComponent<ForceSinPosition>(std::move(entity)) {};

        explicit ForceSinPosition(const Carrot::DocumentElement& doc, Entity entity): ForceSinPosition(std::move(entity)) {
            angularFrequency = DocumentHelpers::read<3, float>(doc["angularFrequency"]);
            amplitude = DocumentHelpers::read<3, float>(doc["amplitude"]);
            angularOffset = DocumentHelpers::read<3, float>(doc["angularOffset"]);
            centerPosition = DocumentHelpers::read<3, float>(doc["centerPosition"]);
        };

        Carrot::DocumentElement serialise() const override {
            Carrot::DocumentElement obj;

            obj["angularFrequency"] = DocumentHelpers::write<3, float>(angularFrequency);
            obj["amplitude"] = DocumentHelpers::write<3, float>(amplitude);
            obj["angularOffset"] = DocumentHelpers::write<3, float>(angularOffset);
            obj["centerPosition"] = DocumentHelpers::write<3, float>(centerPosition);

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