//
// Created by jglrxavpok on 07/04/2022.
//

#include "TextComponent.h"
#include <core/utils/ImGuiUtils.hpp>

namespace Carrot::ECS {
    TextComponent::TextComponent(const rapidjson::Value& json, Entity entity): TextComponent(entity, json["font"].GetString()) {
        setText(json["text"].GetString());
    }

    rapidjson::Value TextComponent::toJSON(rapidjson::Document& doc) const {
        rapidjson::Value obj { rapidjson::kObjectType };
        obj.AddMember("text", rapidjson::Value(text.c_str(), doc.GetAllocator()), doc.GetAllocator());
        obj.AddMember("font", rapidjson::Value((const char*)fontPath.u8string().c_str(), doc.GetAllocator()), doc.GetAllocator());
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