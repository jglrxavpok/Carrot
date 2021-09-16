//
// Created by jglrxavpok on 12/09/2021.
//

#include "LuaScript.h"

namespace Carrot::Lua {

    Script::Script(const Carrot::IO::Resource& resource) {
        // open some common libraries
        open_libraries(sol::lib::base, sol::lib::package);

        registerAllUsertypes(*this);
        safe_script(resource.readText(), sol::script_throw_on_error);
    }

    Script::Script(const std::string& text): Script::Script(Carrot::IO::Resource::inMemory(text)) {}

    Script::Script(const char* text): Script::Script(std::string(text)) {}
}