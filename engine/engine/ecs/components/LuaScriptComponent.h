//
// Created by jglrxavpok on 15/05/2022.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include <core/async/Locks.h>
#include "engine/scripting/LuaScript.h"

namespace Carrot::ECS {
    struct LuaScriptComponent: public IdentifiableComponent<LuaScriptComponent> {
        using ScriptStorage = std::vector<std::pair<IO::VFS::Path, std::unique_ptr<Lua::Script>>>;

        ScriptStorage scripts; // nullptrs are allowed (for invalid paths)

        explicit LuaScriptComponent(Entity entity): IdentifiableComponent<LuaScriptComponent>(std::move(entity)) {}

        explicit LuaScriptComponent(const rapidjson::Value& json, Entity entity);

        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        const char *const getName() const override {
            return "LuaScriptComponent";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override;
    private:
        bool firstTick = true;

        friend class LuaRenderSystem;
        friend class LuaUpdateSystem;
    };
}


template<>
inline const char* Carrot::Identifiable<Carrot::ECS::LuaScriptComponent>::getStringRepresentation() {
    return "LuaScriptComponent";
}
