//
// Created by jglrxavpok on 05/12/2020.
//

#pragma once
#include <string>
#include "engine/constants.h"

namespace Carrot {
    class DebugNameable {
    public:
        template<typename VulkanType>
        static void nameSingle(const std::string& name, const VulkanType& object) {
            vk::DebugUtilsObjectNameInfoEXT nameInfo {
                    .objectType = VulkanType::objectType,
                    .objectHandle = (uint64_t) ((typename VulkanType::CType&) object),
                    .pObjectName = name.c_str(),
            };
            doNaming(nameInfo);
        }

    protected:
        virtual void setDebugNames(const std::string& name) = 0;

    private:
        static void doNaming(const vk::DebugUtilsObjectNameInfoEXT& nameInfo);

        std::string debugName;

    public:
        void name(const std::string& name) {
            setDebugNames(name);
            debugName = name;
        }

        const std::string& getDebugName() const {
            return debugName;
        }

        virtual ~DebugNameable() = default;

        static std::unordered_map<vk::DebugReportObjectTypeEXT, std::unordered_map<std::uint64_t, std::string>> objectNames;
    };
}


