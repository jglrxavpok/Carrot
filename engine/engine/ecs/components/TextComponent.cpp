//
// Created by jglrxavpok on 07/04/2022.
//

#include "TextComponent.h"
#include <core/utils/ImGuiUtils.hpp>

#include "core/io/DocumentHelpers.h"

namespace Carrot::ECS {
    TextComponent::TextComponent(const Carrot::DocumentElement& doc, Entity entity): TextComponent(entity, doc["font"].getAsString()) {
        setText(doc["text"].getAsString());
        if (auto colorIter = doc.getAsObject().find("color"); colorIter.isValid()) {
            setColor(DocumentHelpers::read<4, float>(colorIter->second));
        }
    }

    Carrot::DocumentElement TextComponent::serialise() const {
        Carrot::DocumentElement obj;
        obj["text"] = text;
        obj["font"] = Carrot::toString(fontPath.u8string());
        obj["color"] = DocumentHelpers::write<4, float>(color);
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

    std::string_view TextComponent::getText() const {
        return text;
    }

    void TextComponent::refreshRenderable() {
        if(needsRefresh) {
            if(text.empty()) {
                this->renderableText = std::move(Render::RenderableText{});
            } else {
                this->renderableText = font->bake(Carrot::toU32String(text));
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
