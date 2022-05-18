//
// Created by jglrxavpok on 15/05/2022.
//

#include "LuaScriptComponent.h"
#include "core/io/FileFormats.h"
#include "engine/edition/DragDropTypes.h"
#include <core/utils/ImGuiUtils.hpp>
#include <engine/Engine.h>
#include <engine/utils/Macros.h>
#include <core/io/vfs/VirtualFileSystem.h>

namespace Carrot::ECS {
    LuaScriptComponent::LuaScriptComponent(const rapidjson::Value& json, Entity entity): LuaScriptComponent(entity) {
        for(const auto& element : json["scripts"].GetArray()) {
            const auto& scriptObject = element.GetObject();
            IO::VFS::Path path = IO::VFS::Path(scriptObject["path"].GetString());
            Carrot::IO::Resource resource = path;
            scripts.emplace_back(path, std::make_unique<Lua::Script>(resource));
        }
    }

    rapidjson::Value LuaScriptComponent::toJSON(rapidjson::Document& doc) const {
        rapidjson::Value data{rapidjson::kObjectType};

        rapidjson::Value paths{rapidjson::kArrayType};
        for(const auto& [path, script] : scripts) {
            paths.PushBack(rapidjson::Value(path.toString().c_str(), doc.GetAllocator()), doc.GetAllocator());
        }

        data.AddMember("scripts", paths, doc.GetAllocator());

        return data;
    }

    std::unique_ptr<Component> LuaScriptComponent::duplicate(const Entity& newOwner) const {
        std::unique_ptr<LuaScriptComponent> copy = std::make_unique<LuaScriptComponent>(newOwner);
        for(const auto& [path, script] : scripts) {
            copy->scripts.emplace_back(path, std::make_unique<Lua::Script>(Carrot::IO::Resource(path)));
        }
        return copy;
    }

    void LuaScriptComponent::drawInspectorInternals(const Render::Context& renderContext, bool& modified) {
        ImGui::PushID("LuaScriptComponent");
        std::vector<IO::VFS::Path> toRemove;
        std::size_t index = 0;
        for(auto& [path, _] : scripts) {
            ImGui::PushID(index);

            // TODO: RELOAD BUTTON
            ImGui::Text("[%llu]", index);
            ImGui::SameLine();

            std::string pathStr = path.toString();
            if(ImGui::InputText("Filepath", pathStr, ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::unique_ptr<Lua::Script> script = nullptr;
                IO::VFS::Path vfsPath = IO::VFS::Path(pathStr);
                if(GetVFS().exists(vfsPath)) {
                    script = std::make_unique<Lua::Script>(vfsPath);
                }
                script = std::move(script);
                path = vfsPath;
            }

            if(ImGui::BeginDragDropTarget()) {
                if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::FilePath)) {
                    std::unique_ptr<char8_t[]> buffer = std::make_unique<char8_t[]>(payload->DataSize+sizeof(char8_t));
                    std::memcpy(buffer.get(), static_cast<const char8_t*>(payload->Data), payload->DataSize);
                    buffer.get()[payload->DataSize] = '\0';

                    std::filesystem::path newPath = buffer.get();

                    std::filesystem::path fsPath = std::filesystem::proximate(newPath, std::filesystem::current_path());
                    if(!std::filesystem::is_directory(fsPath) && Carrot::IO::isScriptFormatFromPath(fsPath)) {
                        std::unique_ptr<Lua::Script> script = nullptr;
                        TODO
                        /*
                         * TODO
                        IO::VFS::Path vfsPath = IO::VFS::Path(fsPath);
                        if(GetVFS().exists(vfsPath)) {
                            script = std::make_unique<Lua::Script>(vfsPath);
                        }
                        script = std::move(script);
                        path = vfsPath;
                        */
                        modified = true;
                    }
                }

                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine();
            if(ImGui::Button("x")) {
                toRemove.push_back(path);
            }

            ImGui::PopID();
            index++;
        }

        for(const auto& p : toRemove) {
            std::erase_if(scripts, [&](const auto& pair) { return pair.first == p; });
        }

        if(ImGui::Button("+")) {
            scripts.emplace_back("", nullptr);
        }
        ImGui::PopID();
    }

}