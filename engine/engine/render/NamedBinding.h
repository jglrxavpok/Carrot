//
// Created by jglrxavpok on 21/02/2021.
//

#pragma once
#include "engine/vulkan/includes.h"

namespace Carrot {
    struct NamedBinding {
        vk::DescriptorSetLayoutBinding vkBinding;
        std::string name;

        bool areSame( NamedBinding const& rhs ) const
        {
            return ( rhs.vkBinding.descriptorType == vkBinding.descriptorType )
                   && ( rhs.vkBinding.descriptorCount == vkBinding.descriptorCount )
                   && ( rhs.vkBinding.binding == vkBinding.binding )
                   && ( rhs.vkBinding.pImmutableSamplers == vkBinding.pImmutableSamplers )
                   && ( rhs.name == name );
        }
    };
}
