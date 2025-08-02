//
// Created by jglrxavpok on 07/04/2022.
//

#include "TextComponent.h"
#include <core/utils/ImGuiUtils.hpp>
#include <engine/render/VulkanRenderer.h>

#include "core/io/DocumentHelpers.h"

namespace Carrot::ECS {

    TextComponent::TextComponent(Entity entity, const std::filesystem::path& fontFile)
    : IdentifiableComponent<TextComponent>(std::move(entity)), fontPath(fontFile), font(GetRenderer().getOrCreateFront(fontFile.string())) {};

    TextComponent::TextComponent(const Carrot::DocumentElement& doc, Entity entity): TextComponent(entity, doc["font"].getAsString()) {
        setText(doc["text"].getAsString());
        if (auto colorIter = doc.getAsObject().find("color"); colorIter.isValid()) {
            setColor(DocumentHelpers::read<4, float>(colorIter->second));
        }
        if (auto horizontalAlignmentIter = doc.getAsObject().find("horizontal_alignment"); horizontalAlignmentIter.isValid()) {
            std::string_view valueStr = horizontalAlignmentIter->second.getAsString();
            if (valueStr == "left") {
                setHorizontalAlignment(Render::TextAlignment::Left);
            } else if (valueStr == "center") {
                setHorizontalAlignment(Render::TextAlignment::Center);
            } else {
                TODO; // missing case
            }
        }
    }

    Carrot::DocumentElement TextComponent::serialise() const {
        Carrot::DocumentElement obj;
        obj["text"] = text;
        obj["font"] = Carrot::toString(fontPath.u8string());
        obj["color"] = DocumentHelpers::write<4, float>(color);
        
        switch (horizontalAlignment) {
            case Render::TextAlignment::Left:
                obj["horizontal_alignment"] = "left";
                break;

            case Render::TextAlignment::Center:
                obj["horizontal_alignment"] = "center";
                break;

            default:
                TODO;
        }
        return obj;
    }

    const glm::vec4& TextComponent::getColor() const {
        return color;
    }

    void TextComponent::setColor(const glm::vec4& newColor) {
        previousColor = color;
        color = newColor;
        needsRefresh |= newColor != previousColor;
    }

    Render::TextAlignment TextComponent::getHorizontalAlignment() const {
        return horizontalAlignment;
    }

    void TextComponent::setHorizontalAlignment(Render::TextAlignment newAlignment) {
        needsRefresh |= horizontalAlignment != newAlignment;
        horizontalAlignment = newAlignment;
    }

    std::string_view TextComponent::getText() const {
        return text;
    }

    void TextComponent::refreshRenderable() {
        if(needsRefresh) {
            if(text.empty()) {
                this->renderableText = std::move(Render::RenderableText{});
            } else {
                this->renderableText = font->bake(Carrot::toU32String(text), Render::Font::DefaultPixelSize, horizontalAlignment);
            }
            this->renderableText.getColor() = color;
            needsRefresh = false;
        }
    }

    void TextComponent::setText(std::string_view text) {
        previousText = this->text;
        needsRefresh |= previousText != text;
        this->text = text;
    }
}
