//
// Created by jglrxavpok on 10/10/2021.
//

#include "UUID.h"

namespace Carrot {
    const UUID& UUID::null() {
        static UUID null = UUID(0,0,0,0);
        return null;
    }
}