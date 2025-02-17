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
    SpriteComponent::SpriteComponent(const Carrot::DocumentElement& obj, Entity entity): SpriteComponent::SpriteComponent(std::move(entity)) {
        if (obj.contains("isTransparent")) {
            isTransparent = obj["isTransparent"].getAsBool();
        } else {
            isTransparent = false;
        }

        if(obj.contains("sprite")) {
            auto& spriteData = obj["sprite"];

            if(spriteData.contains("texturePath")) {
                const auto& texturePathJSON = spriteData["texturePath"];
                std::string_view texturePath { texturePathJSON.getAsString() };
                auto textureRef = GetAssetServer().blockingLoadTexture(texturePath);
                float minX = 0;
                float minY = 0;
                float maxX = 1;
                float maxY = 1;
                if (spriteData.contains("region")) {
                    auto regionData = spriteData["region"].getAsArray();
                    minX = regionData[0].getAsDouble();
                    minY = regionData[1].getAsDouble();
                    maxX = regionData[2].getAsDouble();
                    maxY = regionData[3].getAsDouble();
                }

                sprite = std::make_unique<Render::Sprite>(textureRef, Math::Rect2Df(minX, minY, maxX, maxY));
            } else {
                TODO // cannot load non-file textures at the moment
            }
        }
    }

    Carrot::DocumentElement SpriteComponent::serialise() const {
        Carrot::DocumentElement obj;

        obj["isTransparent"] = isTransparent;
        if(sprite) {
            Carrot::DocumentElement spriteData;
            auto& texture = sprite->getTexture();
            auto& resource = texture.getOriginatingResource();

            Carrot::DocumentElement region{DocumentType::Array};
            region.pushBack(sprite->getTextureRegion().getMinX());
            region.pushBack(sprite->getTextureRegion().getMinY());
            region.pushBack(sprite->getTextureRegion().getMaxX());
            region.pushBack(sprite->getTextureRegion().getMaxY());
            spriteData["region"] = region;

            if(resource.isFile()) {
                spriteData["texturePath"] = resource.getName();
            }


            obj["sprite"] = spriteData;
        }

        return obj;
    }
}