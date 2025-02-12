//
// Created by jglrxavpok on 06/04/2022.
//

#pragma once

#include <cstdint>

// Various types that probably don't need an entire file for them.
// Classes & structs should be kept at a minimum here (to avoid long compilation times)
using u64 = std::uint64_t;
using i64 = std::int64_t;
using u32 = std::uint32_t;
using i32 = std::int32_t;
using u16 = std::uint16_t;
using i16 = std::int16_t;
using u8 = std::uint8_t;
using i8 = std::int8_t;

namespace Carrot {
    enum class ShouldRecurse {
        NoRecursion,
        Recursion,
    };

}