//
// Created by jglrxavpok on 20/08/2022.
//

#include "DebugNameable.h"
#include "engine/Engine.h"
#include "engine/vulkan/VulkanDriver.h"

namespace Carrot {
    void DebugNameable::doNaming(const vk::DebugMarkerObjectNameInfoEXT& nameInfo) {
        if(GetVulkanDriver().hasDebugNames()) {

            GetVulkanDriver().getLogicalDevice().debugMarkerSetObjectNameEXT(nameInfo);
        }
    }
}