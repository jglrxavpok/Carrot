//
// Created by jglrxavpok on 19/05/2022.
//

#pragma once

#include <sol/sol.hpp>
#include "engine/ecs/components/Component.h"
#include "engine/ecs/EntityTypes.h"

namespace Carrot::ECS {
    void registerBindings(sol::state& destination);
}
