//
// Created by jglrxavpok on 26/02/2021.
//

#include "System.h"
#include "ecs/World.h"

template<Carrot::SystemType type, typename... RequiredComponents>
Carrot::SignedSystem<type, RequiredComponents...>::SignedSystem(Carrot::World& world): System(world) {
    signature.addComponents<RequiredComponents...>();
}
