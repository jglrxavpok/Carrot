//
// Created by jglrxavpok on 04/12/2020.
//

#pragma once
#include <cstdlib>
#include <core/utils/UUID.h>

namespace Carrot {
    struct GBufferDrawData {
        uint32_t materialIndex;

        uint32_t uuid0 = 0;
        uint32_t uuid1 = 0;
        uint32_t uuid2 = 0;
        uint32_t uuid3 = 0;

        void setUUID(const Carrot::UUID& uuid) {
            uuid0 = uuid.data0();
            uuid1 = uuid.data1();
            uuid2 = uuid.data2();
            uuid3 = uuid.data3();
        }
    };
}
