//
// Created by jglrxavpok on 16/09/2021.
//

#include <sol/sol.hpp>
#include "engine/vulkan/VulkanDriver.h"
#include "engine/Engine.h"

namespace Carrot {
    void VulkanDriver::registerUsertype(sol::state& destination) {
        auto carrotNamespace = destination["Carrot"].get_or_create<sol::table>();
        carrotNamespace.new_usertype<VulkanDriver>("VulkanDriver", sol::no_constructor,
                                                   "getFinalRenderSize", &VulkanDriver::getFinalRenderSize,
                                                   "getEngine", &VulkanDriver::getEngine,
                                                   "getWindowFramebufferExtent", &VulkanDriver::getWindowFramebufferExtent
                // TODO
                                                   );
    }
}