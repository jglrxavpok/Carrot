//
// Created by jglrxavpok on 15/05/2022.
//

#include "LuaSystems.h"
#include <core/io/Logging.hpp>

namespace Carrot::ECS {
    template<typename... Args>
    void runFunction(const IO::VFS::Path& scriptPath, const sol::protected_function& function, Args&&... args) {
        if(function.valid()) {
            auto result = function(std::forward<Args>(args)...);
            if(!result.valid()) {
                sol::error err = result;
                Carrot::Log::error("[%s] Lua execution error: %s", scriptPath.toString().c_str(), err.what());
            }
        }
    }

    void LuaUpdateSystem::tick(double dt) {
        forEachEntity([&](Entity& entity, LuaScriptComponent& component) {
            if(component.firstTick) {
                for(const auto& [p, pScript] : component.scripts) {
                    runFunction(p, (*pScript)["start"], entity);
                }

                component.firstTick = false;
            }

            for(const auto& [p, pScript] : component.scripts) {
                runFunction(p, (*pScript)["eachTick"], entity, dt);
            }
        });
    }

    void LuaUpdateSystem::broadcastStartEvent() {
    }

    void LuaUpdateSystem::broadcastStopEvent() {
        forEachEntity([&](Entity& entity, LuaScriptComponent& component) {
            for(const auto& [p, pScript] : component.scripts) {
                runFunction(p, (*pScript)["stop"], entity);
            }
        });
    }

    void LuaRenderSystem::onFrame(Carrot::Render::Context renderContext) {
        forEachEntity([&](Entity& entity, LuaScriptComponent& component) {
            for(const auto& [p, pScript] : component.scripts) {
                runFunction(p, (*pScript)["eachFrame"], entity, renderContext);
            }
        });
    }
}