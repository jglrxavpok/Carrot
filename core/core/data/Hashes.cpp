//
// Created by jglrxavpok on 16/07/2023.
//

#include "Hashes.h"
#include <robin_hood.h>

namespace Carrot {
    void hash_combine(std::size_t& seed, const std::size_t& v) {
        seed ^= robin_hood::hash_int(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    }
}