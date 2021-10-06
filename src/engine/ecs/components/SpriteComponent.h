//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/render/Sprite.h"

namespace Carrot::ECS {
    struct SpriteComponent: public IdentifiableComponent<SpriteComponent> {
        std::shared_ptr<Carrot::Render::Sprite> sprite;
        bool isTransparent = false;

        explicit SpriteComponent(Entity entity): IdentifiableComponent<SpriteComponent>(std::move(entity)) {}

        explicit SpriteComponent(const rapidjson::Value& json, Entity entity): SpriteComponent(std::move(entity)) {
            TODO
        }

        rapidjson::Value toJSON(rapidjson::Document& doc) const override {
            rapidjson::Value obj{rapidjson::kObjectType};

            obj.AddMember("isTransparent", isTransparent, doc.GetAllocator());
            // TODO: sprite info

            return obj;
        }

        const char *const getName() const override {
            return "SpriteComponent";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<SpriteComponent>(newOwner);
            result->sprite = sprite->duplicate();
            result->isTransparent = isTransparent;
            return result;
        }
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::SpriteComponent>::getStringRepresentation() {
    return "SpriteComponent";
}