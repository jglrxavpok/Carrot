//
// Created by jglrxavpok on 21/02/2021.
//

#pragma once

namespace Carrot {
    struct NamedBinding {
        vk::DescriptorSetLayoutBinding vkBinding;
        u32 setID;
        std::string name;

        bool areSame( NamedBinding const& rhs ) const
        {
            return ( rhs.setID == setID )
                   && ( rhs.vkBinding.descriptorType == vkBinding.descriptorType )
                   && ( rhs.vkBinding.descriptorCount == vkBinding.descriptorCount )
                   && ( rhs.vkBinding.binding == vkBinding.binding )
                   && ( rhs.vkBinding.pImmutableSamplers == vkBinding.pImmutableSamplers );
                   //&& ( rhs.name == name );
        }
    };
}
