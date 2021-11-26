//
// Created by jglrxavpok on 16/09/2021.
//

#include <sol/sol.hpp>
#include "engine/Engine.h"

namespace Carrot {
    void Engine::registerUsertype(sol::state& destination) {
        auto carrotNamespace = destination["Carrot"].get_or_create<sol::table>();
        carrotNamespace.new_usertype<Carrot::Engine>("Engine",
                                                     sol::no_constructor,
                                                     "getRenderer", &Engine::getRenderer,
                                                     "getVulkanDriver", &Engine::getVulkanDriver
                                                     // TODO
                                                     );
    }
}