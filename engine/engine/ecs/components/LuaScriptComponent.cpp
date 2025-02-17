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
    LuaScriptComponent::LuaScriptComponent(const Carrot::DocumentElement& doc, Entity entity): LuaScriptComponent(entity) {
        for(const auto& element : doc["scripts"].getAsArray()) {
            IO::VFS::Path path = IO::VFS::Path(element["path"].getAsString());
            Carrot::IO::Resource resource = path;
            scripts.emplace_back(path, std::make_unique<Lua::Script>(resource));
        }
    }

    Carrot::DocumentElement LuaScriptComponent::serialise() const {
        Carrot::DocumentElement data;

        Carrot::DocumentElement paths{DocumentType::Array};
        for(const auto& [path, script] : scripts) {
            Carrot::DocumentElement scriptObject;
            scriptObject["path"] = path.toString();
            paths.pushBack() = scriptObject;
        }

        data["scripts"] = paths;

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