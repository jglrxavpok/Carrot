//
// Created by jglrxavpok on 12/09/2021.
//

#include "LuaScript.h"
#include "core/io/Logging.hpp"
#include "core/io/vfs/VirtualFileSystem.h"
#include "engine/utils/Macros.h"
#include "engine/Engine.h"

namespace Carrot::Lua {

    static sol::protected_function_result printOnLoadError(lua_State* L, sol::protected_function_result&& result) {
        // from sol2 code
        sol::type t = sol::type_of(L, result.stack_index());
        std::string err = "sol: ";
        err += to_string(result.status());
        err += " error";
        if (t == sol::type::string) {
            err += ": ";
            std::string_view serr = sol::stack::unqualified_get<std::string_view>(L, result.stack_index());
            err.append(serr.data(), serr.size());
        }

        Carrot::Log::error("Failed to load Lua script: %s", err.c_str());
        return std::move(result);
    }

    Script::Script(const Carrot::IO::Resource& resource) {
        // open some common libraries
        open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::string);

        registerAllUsertypes(*this);

        if(resource.isFile()) {
            std::filesystem::path p = GetVFS().resolve(IO::VFS::Path(resource.getName()));
            safe_script_file(p.string(), printOnLoadError);
        } else {
            safe_script(resource.readText(), printOnLoadError);
        }
    }

    Script::Script(const std::string& text): Script::Script(Carrot::IO::Resource::inMemory(text)) {}

    Script::Script(const char* text): Script::Script(std::string(text)) {}
}