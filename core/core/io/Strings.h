#pragma once

#include <string>

namespace Carrot::IO {
    /**
     * Converts a given byte size to a format using B, kiB, MiB, GiB and so on
     * @param size the size to convert
     * @return a std::string with the formatted string
     */
    std::string toReadableFormat(std::uint64_t size);
}