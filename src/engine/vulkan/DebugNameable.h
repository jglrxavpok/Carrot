//
// Created by jglrxavpok on 05/12/2020.
//

#pragma once
#include <string>
#include "engine/constants.h"
#include "engine/vulkan/includes.h"
#include "engine/vulkan/VulkanDriver.h"

namespace Carrot {
    class DebugNameable {
    protected:
        virtual void setDebugNames(const std::string& name) = 0;

        template<typename VulkanType>
        void nameSingle(VulkanDriver& driver, const std::string& name, const VulkanType& object) {
#ifndef NO_DEBUG
            if(USE_DEBUG_MARKERS) {
                vk::DebugMarkerObjectNameInfoEXT nameInfo {
                        .objectType = VulkanType::debugReportObjectType,
                    //    .object = (uint64_t) ((VulkanType::CType&) object),
                        .pObjectName = name.c_str(),
                };
                driver.getLogicalDevice().debugMarkerSetObjectNameEXT(nameInfo);
            }
#endif
        }

    public:
        void name(const std::string& name) {
            setDebugNames(name);
        }

        virtual ~DebugNameable() = default;
    };
}


