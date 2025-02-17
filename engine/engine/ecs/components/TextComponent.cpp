//
// Created by jglrxavpok on 07/04/2022.
//

#include "TextComponent.h"
#include <core/utils/ImGuiUtils.hpp>

namespace Carrot::ECS {
    TextComponent::TextComponent(const Carrot::DocumentElement& doc, Entity entity): TextComponent(entity, doc["font"].getAsString()) {
        setText(doc["text"].getAsString());
    }

    Carrot::DocumentElement TextComponent::serialise() const {
        Carrot::DocumentElement obj;
        obj["text"] = text;
        obj["font"] = Carrot::toString(fontPath.u8string());
        return obj;
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
            needsRefresh = false;
        }
    }

    void TextComponent::setText(std::string_view text) {
        previousText = this->text;
        needsRefresh |= previousText != text;
        this->text = text;
    }
}