//
// Created by jglrxavpok on 07/04/2022.
//

#pragma once

#include "Component.h"
#include "engine/render/resources/Font.h"

namespace Carrot::ECS {
    /// Not meant for quickly changing text
    class TextComponent : public IdentifiableComponent<TextComponent> {
    public:
        explicit TextComponent(Entity entity, const std::filesystem::path& fontFile = "resources/fonts/Roboto-Medium.ttf"): IdentifiableComponent<TextComponent>(std::move(entity)), fontPath(fontFile), font(GetRenderer().getOrCreateFront(fontFile.string())) {};

        explicit TextComponent(const rapidjson::Value& json, Entity entity);

    public:
        std::string_view getText() const;
        void setText(std::string_view text);

    public:
        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        const char *const getName() const override {
            return "Text";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<TextComponent>(newOwner, fontPath);
            result->setText(getText());
            return result;
        }

    private:
        void refreshRenderable();

    private:
        bool needsRefresh = false;
        std::string text;
        std::string previousText;
        std::filesystem::path fontPath;
        std::shared_ptr<Carrot::Render::Font> font;
        Carrot::Render::RenderableText renderableText;

        friend class TextRenderSystem;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::TextComponent>::getStringRepresentation() {
    return "Text";
}
