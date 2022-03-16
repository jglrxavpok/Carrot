//
// Created by jglrxavpok on 26/02/2021.
//

#include "System.h"
#include "engine/ecs/World.h"

namespace Carrot::ECS {
    template<SystemType type, typename... RequiredComponents>
    SignedSystem<type, RequiredComponents...>::SignedSystem(World& world): System(world) {
        signature.addComponents<RequiredComponents...>();
    }

}
