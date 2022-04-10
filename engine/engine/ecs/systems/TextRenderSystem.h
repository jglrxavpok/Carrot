//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/TextComponent.h>

namespace Carrot::ECS {
    class TextRenderSystem: public RenderSystem<TransformComponent, Carrot::ECS::TextComponent>, public Identifiable<TextRenderSystem> {
    public:
        explicit TextRenderSystem(World& world): RenderSystem<TransformComponent, TextComponent>(world) {}
        explicit TextRenderSystem(const rapidjson::Value& json, World& world): TextRenderSystem(world) {}

        void onFrame(Carrot::Render::Context renderContext) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;

    public:
        inline static const char* getStringRepresentation() {
            return "TextRender";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }
    };
}
