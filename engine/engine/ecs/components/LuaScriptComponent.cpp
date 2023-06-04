//
// Created by jglrxavpok on 15/05/2022.
//

#include "LuaScriptComponent.h"
#include "core/io/FileFormats.h"
#include "engine/edition/DragDropTypes.h"
#include "core/io/Logging.hpp"
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
            rapidjson::Value scriptObject{rapidjson::kObjectType};
            scriptObject.AddMember("path", rapidjson::Value(path.toString().c_str(), doc.GetAllocator()), doc.GetAllocator());
            paths.PushBack(scriptObject, doc.GetAllocator());
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

}