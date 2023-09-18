//
// Created by jglrxavpok on 28/11/2021.
//

#pragma once

#include <functional>
#include <string>
#include <filesystem>
#include <utility>
#include "core/utils/stringmanip.h"
#include "core/tasks/Tasks.h"

namespace std {
    template<>
    struct hash<Carrot::Async::TaskLane> {
        std::size_t operator()(const Carrot::Async::TaskLane& toHash) const {
            return toHash.id;
        }
    };

    template<>
    struct hash<std::pair<std::string, std::uint64_t>> {
        std::size_t operator()(const std::pair<std::string, std::uint64_t>& p) const {
            const std::size_t prime = 31;
            std::hash<std::string> stringHasher;

            std::size_t hash = stringHasher(p.first);
            hash = p.second + hash * prime;
            return hash;
        }
    };
}


namespace Carrot {
    void hash_combine(std::size_t& seed, const std::size_t& v);
}
