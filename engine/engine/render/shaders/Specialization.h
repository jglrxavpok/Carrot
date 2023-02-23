//
// Created by jglrxavpok on 04/12/2020.
//

#pragma once

namespace Carrot {
    struct Specialization {
        uint32_t offset = 0;
        uint32_t size = 0;
        uint32_t constantID = 0;

        vk::SpecializationMapEntry convertToEntry() const;
    };

}
