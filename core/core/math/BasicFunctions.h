//
// Created by jglrxavpok on 05/08/2021.
//

#pragma once

#include "core/utils/Assert.h"

namespace Carrot::Math {
    template<typename Scalar>
    inline Scalar imod(Scalar input, Scalar mod) {
        Scalar tmp = input % mod;
        if(input < 0)
            return mod + tmp;
        return tmp;
    }

    template<typename Scalar>
    inline constexpr bool isPowerOf2(Scalar value) {
        return value > 0 && !(value & (value - 1));
    }

    // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    inline constexpr std::uint32_t nextPowerOf2(std::uint32_t v) {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;
        return v;
    }

    template<typename Scalar>
    inline constexpr Scalar alignUp(Scalar value, Scalar alignment) {
        verify(isPowerOf2(alignment), "alignment must be a power of 2");
        return (value + alignment - 1) & ~(alignment - 1);
    }

    template<typename Scalar>
    inline constexpr Scalar alignDown(Scalar value, Scalar alignment) {
        verify(isPowerOf2(alignment), "alignment must be a power of 2");
        return (value / alignment) * alignment;
    }
}