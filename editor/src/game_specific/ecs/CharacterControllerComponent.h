//
// Created by jglrxavpok on 24/03/2022.
//

#pragma once

#include <engine/ecs/components/Component.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <core/utils/JSON.h>
#include "engine/render/Viewport.h"
#include <imgui.h>

namespace Game::ECS {
    class CharacterControllerComponent: public Carrot::ECS::IdentifiableComponent<CharacterControllerComponent> {
    public:
        explicit CharacterControllerComponent(Carrot::ECS::Entity entity): Carrot::ECS::IdentifiableComponent<CharacterControllerComponent>(std::move(entity)) {};

        explicit CharacterControllerComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity): CharacterControllerComponent(std::move(entity)) {

        };

        rapidjson::Value toJSON(rapidjson::Document& doc) const override {
            rapidjson::Value obj(rapidjson::kObjectType);

            return obj;
        }

        const char *const getName() const override {
            return "CharacterController";
        }

        std::unique_ptr<Carrot::ECS::Component> duplicate(const Carrot::ECS::Entity& newOwner) const override {
            auto result = std::make_unique<CharacterControllerComponent>(newOwner);
            return result;
        }

        void drawInspectorInternals(const Carrot::Render::Context& renderContext, bool& modified) override;
    };
}

template<>
inline const char* Carrot::Identifiable<Game::ECS::CharacterControllerComponent>::getStringRepresentation() {
    return "CharacterController";
}