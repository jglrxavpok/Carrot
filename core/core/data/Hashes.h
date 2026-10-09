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

namespace Carrot {
    void hash_combine(std::size_t& seed, const std::size_t& v) noexcept;
}

namespace std {
    template<>
    struct hash<Carrot::Async::TaskLane> {
        std::size_t operator()(const Carrot::Async::TaskLane& toHash) const noexcept {
            return toHash.id;
        }
    };

    template<>
    struct hash<std::pair<std::string, std::uint64_t>> {
        std::size_t operator()(const std::pair<std::string, std::uint64_t>& p) const noexcept {
            const std::size_t prime = 31;
            std::hash<std::string> stringHasher;

            std::size_t hash = stringHasher(p.first);
            hash = p.second + hash * prime;
            return hash;
        }
    };

    template<typename TKey, typename TValue>
    struct hash<std::unordered_map<TKey, TValue>> {
        std::size_t operator()(const std::unordered_map<TKey, TValue>& map) const noexcept {
            std::hash<TKey> keyHasher;
            std::hash<TValue> valueHasher;
            std::size_t h = 0;
            for (const auto& [key, value] : map) {
                Carrot::hash_combine(h, keyHasher(key));
                Carrot::hash_combine(h, valueHasher(value));
            }
            return h;
        }
    };
}
