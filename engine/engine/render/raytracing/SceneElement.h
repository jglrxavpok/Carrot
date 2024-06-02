//
// Created by jglrxavpok on 01/03/2021.
//

#pragma once

#include <cstdint>

namespace Carrot {

    enum class BLASGeometryFormat: std::uint8_t;

    namespace SceneDescription {
        struct Geometry {
            vk::DeviceAddress vertexBufferAddress = (vk::DeviceAddress)-1;
            vk::DeviceAddress indexBufferAddress = (vk::DeviceAddress)-1;
            std::uint32_t materialIndex = (std::uint32_t)-1;
            BLASGeometryFormat geometryFormat = (BLASGeometryFormat)-1; // See Carrot::BLASGeometryFormat
        };

        struct Instance {
            glm::vec4 instanceColor{1,1,1,1};
            std::uint32_t firstGeometryIndex = (std::uint32_t)-1;
        };
    }

}