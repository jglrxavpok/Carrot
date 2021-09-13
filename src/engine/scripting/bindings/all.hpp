#pragma once
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include "glm.hpp"

namespace Carrot::Lua {
    inline void registerAllUsertypes(sol::state& destination) {
        registerGLMUsertypes(destination);
    }
}