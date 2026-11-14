//
// Created by jglrxavpok on 14/11/2026.
//

#pragma once

#include <array>

constexpr i64 MaxCallstackDepth = 64;

namespace Carrot::Debug {
    struct CallstackEntry {
        std::string filename;
        std::string function;
        i32 line = -1;
    };

    /**
     * Represents a callstack with the address of each current function
     * The callstack is computed when the constructor is called for ease of use
     */
    struct Callstack {
        Callstack();

        std::array<CallstackEntry, MaxCallstackDepth> entries;
    };
}
