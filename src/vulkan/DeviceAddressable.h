//
// Created by jglrxavpok on 25/02/2021.
//

#pragma once

#include <vulkan/includes.h>

namespace Carrot {
    struct DeviceAddressable {
        virtual vk::DeviceAddress getDeviceAddress() const = 0;
    };
}