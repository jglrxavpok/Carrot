//
// Created by jglrxavpok on 11/09/2021.
//

#pragma once

namespace Carrot::Concepts {
    template<typename A, typename B>
    concept Same = std::is_same_v<A, B>;

    template<typename A, typename B>
    concept NotSame = !std::is_same_v<A, B>;
}