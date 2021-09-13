//
// Created by jglrxavpok on 12/09/2021.
//

#include "LuaScript.h"

namespace Carrot::Lua {

    Script::Script(const Carrot::IO::Resource& resource) {
        // open some common libraries
        luaState.open_libraries(sol::lib::base, sol::lib::package);

        registerAllUsertypes(luaState);
        std::string code = resource.readText();
        std::cout << code << std::endl;
        luaState.script(code);
    }

    Script::Script(const std::string& text): Script::Script(Carrot::IO::Resource::inMemory(text)) {}

    Script::Script(const char* text): Script::Script(std::string(text)) {}

    sol::state& Script::getLuaState() {
        return luaState;
    }
}