//
// Created by jglrxavpok on 05/08/2021.
//

#pragma once

namespace Carrot::Math {
    template<typename Scalar>
    inline Scalar imod(Scalar input, Scalar mod) {
        Scalar tmp = input % mod;
        if(input < 0)
            return mod + tmp;
        return tmp;
    }
}