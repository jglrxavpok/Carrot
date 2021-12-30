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

    template<typename Scalar>
    inline constexpr Scalar alignUp(Scalar value, Scalar alignment) {
        verify(isPowerOf2(alignment), "alignment must be a power of 2");
        return (value + alignment - 1) & ~(alignment - 1);
    }
}