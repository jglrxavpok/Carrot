//
// Created by jglrxavpok on 21/02/2021.
//

#pragma once
#include "vulkan/vulkan.h"

namespace Carrot {
    struct NamedBinding {
        vk::DescriptorSetLayoutBinding vkBinding;
        std::string name;
    };
}
