//
// Created by jglrxavpok on 31/07/2024.
//

#pragma once

#include <cstdint>
#include <core/data/Hashes.h>

namespace Carrot {
    template<typename A, typename B>
    struct Pair {
        A first;
        B second;

        auto operator<=>(const Pair &) const = default;
    };
}

template<typename A, typename B>
struct std::hash<Carrot::Pair<A, B>> {
    std::size_t operator()(const Carrot::Pair<A, B>& pair) const {
        std::size_t hash = std::hash<A>{}(pair.first);
        const std::size_t hashB = std::hash<B>{}(pair.second);
        Carrot::hash_combine(hash, hashB);
        return hash;
    }
};