//
// Created by jglrxavpok on 01/03/2021.
//

#pragma once

#include <cstdint>

namespace Carrot::SceneDescription {

    struct Geometry {
        vk::DeviceAddress vertexBufferAddress = (vk::DeviceAddress)-1;
        vk::DeviceAddress indexBufferAddress = (vk::DeviceAddress)-1;
        std::uint32_t materialIndex = (std::uint32_t)-1;
        std::uint32_t _pad = (std::uint32_t)-1;
    };

    struct Instance {
        std::uint32_t firstGeometryIndex = (std::uint32_t)-1;
    };
}