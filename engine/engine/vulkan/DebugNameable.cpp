//
// Created by jglrxavpok on 20/08/2022.
//

#include "DebugNameable.h"
#include "engine/Engine.h"
#include "engine/vulkan/VulkanDriver.h"

namespace Carrot {
    std::unordered_map<vk::DebugReportObjectTypeEXT, std::unordered_map<std::uint64_t, std::string>> DebugNameable::objectNames{};

    /* static */ void DebugNameable::doNaming(const vk::DebugUtilsObjectNameInfoEXT& nameInfo) {
      //  objectNames[nameInfo.objectType][nameInfo.object] = nameInfo.pObjectName;
        if(GetVulkanDriver().hasDebugNames()) {
            GetVulkanDriver().getLogicalDevice().setDebugUtilsObjectNameEXT(nameInfo);
        }
    }
}