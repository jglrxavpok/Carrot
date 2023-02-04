//
// Created by jglrxavpok on 05/12/2020.
//

#pragma once
#include <string>
#include "engine/constants.h"
#include "engine/vulkan/includes.h"

namespace Carrot {
    class DebugNameable {
    protected:
        virtual void setDebugNames(const std::string& name) = 0;

        template<typename VulkanType>
        void nameSingle(const std::string& name, const VulkanType& object) {
            vk::DebugMarkerObjectNameInfoEXT nameInfo {
                    .objectType = VulkanType::debugReportObjectType,
                    .object = (uint64_t) ((typename VulkanType::CType&) object),
                    .pObjectName = name.c_str(),
            };
            doNaming(nameInfo);
        }

    private:
        void doNaming(const vk::DebugMarkerObjectNameInfoEXT& nameInfo);

    public:
        void name(const std::string& name) {
            setDebugNames(name);
        }

        virtual ~DebugNameable() = default;

        static std::unordered_map<vk::DebugReportObjectTypeEXT, std::unordered_map<std::uint64_t, std::string>> objectNames;
    };
}


