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
    class PageComponent: public Carrot::ECS::IdentifiableComponent<PageComponent> {
    public:
        explicit PageComponent(Carrot::ECS::Entity entity): Carrot::ECS::IdentifiableComponent<PageComponent>(std::move(entity)) {};

        explicit PageComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity): PageComponent(entity) {}

        rapidjson::Value toJSON(rapidjson::Document& doc) const override {
            return rapidjson::Value(rapidjson::kObjectType);
        }

        const char *const getName() const override {
            return "CollectiblePage";
        }

        std::unique_ptr<Carrot::ECS::Component> duplicate(const Carrot::ECS::Entity& newOwner) const override {
            auto result = std::make_unique<PageComponent>(newOwner);
            return result;
        }
    };
}

template<>
inline const char* Carrot::Identifiable<Game::ECS::PageComponent>::getStringRepresentation() {
    return "CollectiblePage";
}