//
// Created by jglrxavpok on 28/11/2021.
//

#pragma once

#include <xhash>
#include <string>
#include <filesystem>
#include "core/utils/stringmanip.h"

namespace std {
    template<>
    struct hash<std::filesystem::path> {
        std::size_t operator()(const std::filesystem::path& toHash) const {
            return std::filesystem::hash_value(toHash);
        }
    };
}
