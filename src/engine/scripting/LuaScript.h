//
// Created by jglrxavpok on 12/09/2021.
//

#pragma once

#include "bindings/all.hpp"
#include <engine/io/Resource.h>

namespace Carrot::Lua {
    class Script {
    public:
        /// Creates a script object from a resource (file or in-memory)
        explicit Script(const Carrot::IO::Resource& script);

        /// Creates a script object directly from raw text
        Script(const std::string& text);

        /// Creates a script object directly from raw text
        Script(const char* text);

        sol::state& getLuaState();

    private:
        sol::state luaState;
    };
}
