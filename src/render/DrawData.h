//
// Created by jglrxavpok on 04/12/2020.
//

#pragma once
#include <cstdlib>

namespace Carrot {
    struct DrawData {
        uint32_t materialIndex;
    };

    struct MaterialData {
        uint32_t textureIndex;
        uint32_t ignoreInstanceColor;
    };
}
