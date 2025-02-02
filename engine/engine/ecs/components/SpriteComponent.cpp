//
// Created by jglrxavpok on 08/10/2021.
//

#include "SpriteComponent.h"
#include <engine/assets/AssetServer.h>
#include "core/utils/ImGuiUtils.hpp"
#include "imgui.h"
#include "engine/edition/DragDropTypes.h"
#include <filesystem>

namespace Carrot::ECS {
    SpriteComponent::SpriteComponent(const rapidjson::Value& json, Entity entity): SpriteComponent::SpriteComponent(std::move(entity)) {
        auto obj = json.GetObject();
        if (obj.HasMember("isTransparent")) {
            isTransparent = obj["isTransparent"].GetBool();
        } else {
            isTransparent = false;
        }

        if(obj.HasMember("sprite")) {
            auto spriteData = obj["sprite"].GetObject();

            if(spriteData.HasMember("texturePath")) {
                const auto& texturePathJSON = spriteData["texturePath"];
                std::string_view texturePath { texturePathJSON.GetString(), texturePathJSON.GetStringLength() };
                auto textureRef = GetAssetServer().blockingLoadTexture(texturePath);
                float minX = 0;
                float minY = 0;
                float maxX = 1;
                float maxY = 1;
                if (spriteData.HasMember("region")) {
                    auto regionData = spriteData["region"].GetArray();
                    minX = regionData[0].GetFloat();
                    minY = regionData[1].GetFloat();
                    maxX = regionData[2].GetFloat();
                    maxY = regionData[3].GetFloat();
                }

                sprite = std::make_unique<Render::Sprite>(textureRef, Math::Rect2Df(minX, minY, maxX, maxY));
            } else {
                TODO // cannot load non-file textures at the moment
            }
        }
    }

    rapidjson::Value SpriteComponent::toJSON(rapidjson::Document& doc) const {
        rapidjson::Value obj{rapidjson::kObjectType};

        obj.AddMember("isTransparent", isTransparent, doc.GetAllocator());
        if(sprite) {
            rapidjson::Value spriteData(rapidjson::kObjectType);
            auto& texture = sprite->getTexture();
            auto& resource = texture.getOriginatingResource();

            rapidjson::Value region{rapidjson::kArrayType};
            region.PushBack(sprite->getTextureRegion().getMinX(), doc.GetAllocator());
            region.PushBack(sprite->getTextureRegion().getMinY(), doc.GetAllocator());
            region.PushBack(sprite->getTextureRegion().getMaxX(), doc.GetAllocator());
            region.PushBack(sprite->getTextureRegion().getMaxY(), doc.GetAllocator());
            spriteData.AddMember("region", region, doc.GetAllocator());

            if(resource.isFile()) {
                rapidjson::Value texturePath{resource.getName(), doc.GetAllocator()};
                spriteData.AddMember("texturePath", texturePath, doc.GetAllocator());
            }


            obj.AddMember("sprite", spriteData, doc.GetAllocator());
        }

        return obj;
    }
}