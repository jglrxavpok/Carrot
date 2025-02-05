//
// Created by jglrxavpok on 17/09/2023.
//

#include "EntityTypes.h"
#include <core/utils/Lookup.hpp>
#include <core/utils/stringmanip.h>
#include <core/io/Logging.hpp>

namespace Carrot::ECS {
    static Carrot::Lookup FlagsNames = std::array {
            Carrot::LookupEntry<EntityFlags>(EntityFlags::None, "None"),
            Carrot::LookupEntry<EntityFlags>(EntityFlags::Hidden, "Hidden"),
    };

    EntityFlags stringToFlags(const std::string& str) {
        EntityFlags flags = EntityFlags::None;
        for(const auto& flag : Carrot::splitString(str, "|")) {
            const EntityFlags* pFlagValue = FlagsNames.find(flag);
            if(!pFlagValue) {
                Carrot::Log::error("Tried to convert string flag '%s' to EntityFlags, but this string is not known", flag.c_str());
            } else {
                flags |= *pFlagValue;
            }
        }
        return flags;
    }

    std::string flagsToString(const EntityFlags& flags) {
        if(flags == EntityFlags::None) {
            return "None";
        }

        std::string result;
        int index = 0;
        for(const auto& [flagBit, name] : FlagsNames) {
            if((flags & flagBit) != 0) {
                if(index != 0) {
                    result += '|';
                }
                result += name;

                index++;
            }
        }
        return result;
    }

    bool isIllegalEntityName(std::string_view entityName) {
        if (entityName == ".LogicSystems" || entityName == ".RenderSystems") {
            return true;
        }
        return false;
    }
}