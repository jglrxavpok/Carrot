//
// Created by jglrxavpok on 04/12/2020.
//

#include "Specialization.h"

vk::SpecializationMapEntry Carrot::Specialization::convertToEntry() const {
    return {
        .constantID = constantID,
        .offset = offset,
        .size = size,
    };
}
