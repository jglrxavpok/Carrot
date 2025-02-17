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

        explicit SpriteComponent(const Carrot::DocumentElement& doc, Entity entity);

        Carrot::DocumentElement serialise() const override;

        const char *const getName() const override {
            return "SpriteComponent";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<SpriteComponent>(newOwner);
            if(sprite) {
                result->sprite = sprite->duplicate();
            }
            result->isTransparent = isTransparent;
            return result;
        }
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::SpriteComponent>::getStringRepresentation() {
    return "SpriteComponent";
}