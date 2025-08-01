//
// Created by jglrxavpok on 22/03/2021.
//

#pragma once
#include <string>
#include <string_view>
#include <engine/utils/Macros.h>

namespace Carrot::Skybox {
    enum class Type {
        None,
        Forest,
        Meadow,
        Clouds,
    };

    enum class Direction {
        NegativeX,
        PositiveX,
        NegativeY,
        PositiveY,
        NegativeZ,
        PositiveZ,
    };
    
    static std::string getSuffix(Direction direction) {
        switch (direction) {
            case Direction::NegativeX:
                return "nx";
            case Direction::PositiveX:
                return "px";
            case Direction::NegativeY:
                return "ny";
            case Direction::PositiveY:
                return "py";
            case Direction::NegativeZ:
                return "nz";
            case Direction::PositiveZ:
                return "pz";

                default:
                TODO
        }
    }
    
    static std::string getTexturePath(Type skyboxType, Direction direction) {
        switch(skyboxType) {
            case Type::None:
                return "";
                
            case Type::Forest:
                return std::string("resources/textures/forest_cubemap/")+getSuffix(direction)+".png";

            case Type::Clouds:
                return std::string("resources/textures/clouds/")+getSuffix(direction)+".png";

            default:
                TODO
        }
    }

    static const char* getName(Type skyboxType) {
        switch(skyboxType) {
            case Type::None:
                return "None";

            case Type::Forest:
                return "Forest";

            case Type::Meadow:
                return "Meadow";

            case Type::Clouds:
                return "Clouds";

            default:
                TODO
        }
    }

    static bool safeFromName(std::string_view name, Type& outSkyboxType) {
        if(name == "None") {
            outSkyboxType = Type::None;
            return true;
        } else if(name == "Forest") {
            outSkyboxType = Type::Forest;
            return true;
        } else if(name == "Meadow") {
            outSkyboxType = Type::Meadow;
            return true;
        } else if(name == "Clouds") {
            outSkyboxType = Type::Clouds;
            return true;
        }
        return false;
    }
}