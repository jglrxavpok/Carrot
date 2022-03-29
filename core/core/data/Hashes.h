//
// Created by jglrxavpok on 28/11/2021.
//

#pragma once

#include <functional>
#include <string>
#include <filesystem>
#include "core/utils/stringmanip.h"
#include "core/tasks/Tasks.h"

namespace std {
    template<>
    struct hash<std::filesystem::path> {
        std::size_t operator()(const std::filesystem::path& toHash) const {
            return std::filesystem::hash_value(toHash);
        }
    };

    template<>
    struct hash<Carrot::Async::TaskLane> {
        std::size_t operator()(const Carrot::Async::TaskLane& toHash) const {
            return toHash.id;
        }
    };
}
