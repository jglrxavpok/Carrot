#pragma once

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace Carrot::Lua {
    void registerAllUsertypes(sol::state& destination);
}