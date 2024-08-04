//
// Created by jglrxavpok on 31/07/2024.
//

#pragma once

#include <cstdint>

namespace Carrot {
    /**
     * Header, as defined by Vulkan, of serialized acceleration structures
     */
    struct VkAccelerationStructureHeader {
        char driverUUID[16 /*VK_UUID_SIZE*/];
        char compatibilityData[16 /*VK_UUID_SIZE*/];
        std::uint64_t accelerationStructureSerializedSize;
        std::uint64_t accelerationStructureRuntimeSize;
        std::uint64_t followingAccelerationStructureHandleCount;
    };
}