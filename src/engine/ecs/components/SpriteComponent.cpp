//
// Created by jglrxavpok on 08/10/2021.
//

#include "SpriteComponent.h"
#include "engine/Engine.h"
#include "engine/utils/ImGuiUtils.hpp"
#include "imgui.h"
#include "engine/edition/DragDropTypes.h"
#include <filesystem>

namespace Carrot::ECS {
    SpriteComponent* SpriteComponent::inInspector = nullptr;

    SpriteComponent::SpriteComponent(const rapidjson::Value& json, Entity entity): SpriteComponent::SpriteComponent(std::move(entity)) {
        auto obj = json.GetObject();
        isTransparent = obj["isTransparent"].GetBool();

        if(obj.HasMember("sprite")) {
            auto spriteData = obj["sprite"].GetObject();

            if(spriteData.HasMember("texturePath")) {
                std::string texturePath = spriteData["texturePath"].GetString();
                auto textureRef = Engine::getInstance().getRenderer().getOrCreateTextureFullPath(texturePath);
                auto regionData = spriteData["region"].GetArray();
                float minX = regionData[0].GetFloat();
                float minY = regionData[1].GetFloat();
                float maxX = regionData[2].GetFloat();
                float maxY = regionData[3].GetFloat();

                sprite = std::make_unique<Render::Sprite>(Engine::getInstance().getRenderer(), textureRef, Math::Rect2Df(minX, minY, maxX, maxY));
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

    void SpriteComponent::drawInspectorInternals(const Render::Context& renderContext, bool& modified) {
        static std::string path = "<<path>>";
        if(inInspector != this) {
            inInspector = this;
            path = sprite->getTexture().getOriginatingResource().getName();
        }
        if(ImGui::InputText("Filepath##SpriteComponent filepath inspector", path, ImGuiInputTextFlags_EnterReturnsTrue)) {
            sprite->setTexture(Engine::getInstance().getRenderer().getOrCreateTextureFullPath(path));
            modified = true;
        }
        if(ImGui::BeginDragDropTarget()) {
            if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::FilePath)) {
                std::unique_ptr<char8_t[]> buffer = std::make_unique<char8_t[]>(payload->DataSize+1);
                std::memcpy(buffer.get(), static_cast<const void*>(payload->Data), payload->DataSize);
                buffer.get()[payload->DataSize] = '\0';

                std::u8string newPath = buffer.get();

                std::filesystem::path fsPath = std::filesystem::proximate(newPath, std::filesystem::current_path());
                if(!std::filesystem::is_directory(fsPath) && Carrot::IO::isImageFormatFromPath(fsPath)) {
                    sprite->setTexture(Engine::getInstance().getRenderer().getOrCreateTextureFullPath(fsPath.string().c_str()));
                    inInspector = nullptr;
                    modified = true;
                }
            }

            ImGui::EndDragDropTarget();
        }

        auto& region = sprite->getTextureRegion();
        bool recompute = false;
        float minU = region.getMinX();
        float minV = region.getMinY();
        float maxU = region.getMaxX();
        float maxV = region.getMaxY();
        if(ImGui::InputFloat("Min U##SpriteComponent minU inspector", &minU)
        |  ImGui::InputFloat("Min V##SpriteComponent minV inspector", &minV)
        |  ImGui::InputFloat("Max U##SpriteComponent maxU inspector", &maxU)
        |  ImGui::InputFloat("Max V##SpriteComponent maxV inspector", &maxV)) {
            recompute = true;
        }

        if(recompute) {
            modified = true;
            region = Math::Rect2Df(minU, minV, maxU, maxV);
        }
    }
}