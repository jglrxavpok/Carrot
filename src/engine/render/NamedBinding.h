//
// Created by jglrxavpok on 21/02/2021.
//

#pragma once
#include "engine/vulkan/includes.h"

namespace Carrot {
    struct NamedBinding {
        vk::DescriptorSetLayoutBinding vkBinding;
        std::string name;
    };
}
