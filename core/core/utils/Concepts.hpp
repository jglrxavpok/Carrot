//
// Created by jglrxavpok on 11/09/2021.
//

#pragma once

#include <type_traits>

namespace Carrot::Concepts {
    template<typename A, typename B>
    concept Same = std::is_same_v<A, B>;

    template<typename A, typename B>
    concept NotSame = !std::is_same_v<A, B>;

    template<typename T>
    concept IsMoveable = std::is_move_constructible_v<T>;

    template<typename T>
    concept Hashable = requires(T a)
    {
        { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
    };
}