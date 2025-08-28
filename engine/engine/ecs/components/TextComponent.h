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
        explicit TextComponent(Entity entity, const std::filesystem::path& fontFile = "resources/fonts/Roboto-Medium.ttf");

        explicit TextComponent(const Carrot::DocumentElement& json, Entity entity);

    public:
        std::string_view getText() const;
        void setText(std::string_view text);

        const glm::vec4& getColor() const;
        void setColor(const glm::vec4& newColor);

        Render::TextAlignment getHorizontalAlignment() const;
        void setHorizontalAlignment(Render::TextAlignment newAlignment);

    public:
        Carrot::DocumentElement serialise() const override;

        const char *const getName() const override {
            return "Text";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<TextComponent>(newOwner, fontPath);
            result->setColor(getColor());
            result->setText(getText());
            result->setHorizontalAlignment(getHorizontalAlignment());
            return result;
        }

        Render::RenderableText& getRenderableText(const Carrot::Render::Context& currentFrame);

    private:
        void refreshRenderable(const Carrot::Render::Context& currentFrame);

    private:
        Render::PerFrame<bool> needsRefreshs;
        std::string text;
        std::string previousText;
        glm::vec4 color{1.0f};
        glm::vec4 previousColor{1.0f};
        std::filesystem::path fontPath;
        std::shared_ptr<Carrot::Render::Font> font;
        Render::PerFrame<Carrot::Render::RenderableText> renderableTexts;
        Render::TextAlignment horizontalAlignment = Render::TextAlignment::Center;

        friend class TextRenderSystem;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::TextComponent>::getStringRepresentation() {
    return "Text";
}
