//
// Created by jglrxavpok on 02/09/2021.
//

#include "Serialisation.h"
#include <engine/utils/Macros.h>

namespace Carrot::IO {
    std::uint8_t VectorReader::next() {
        if(ptr >= data.size()) {
            throw std::runtime_error("Tried to read past packet length (" + std::to_string(data.size()) + ")!");
        }
        return data[ptr++];
    }

    VectorReader& VectorReader::operator>>(std::uint8_t& out) {
        out = next();
        return *this;
    }

    VectorReader& VectorReader::operator>>(char& out) {
        out = std::bit_cast<char>(next());
        return *this;
    }

    VectorReader& VectorReader::operator>>(std::uint16_t& out) {
        auto v = static_cast<std::uint16_t>(next());
        v |= static_cast<std::uint16_t>(next()) << 8;
        out = v;
        return *this;
    }

    VectorReader& VectorReader::operator>>(std::uint32_t& out) {
        auto v = static_cast<std::uint32_t>(next());
        v |= static_cast<std::uint32_t>(next()) << 8;
        v |= static_cast<std::uint32_t>(next()) << 16;
        v |= static_cast<std::uint32_t>(next()) << 24;
        out = v;
        return *this;
    }

    VectorReader& VectorReader::operator>>(std::uint64_t& out) {
        auto v = static_cast<std::uint64_t>(next());
        v |= static_cast<std::uint64_t>(next()) << 8;
        v |= static_cast<std::uint64_t>(next()) << 16;
        v |= static_cast<std::uint64_t>(next()) << 24;
        v |= static_cast<std::uint64_t>(next()) << 32;
        v |= static_cast<std::uint64_t>(next()) << 40;
        v |= static_cast<std::uint64_t>(next()) << 48;
        v |= static_cast<std::uint64_t>(next()) << 56;
        out = v;
        return *this;
    }

    VectorReader& VectorReader::operator>>(float& out) {
        std::uint32_t v;
        *this >> v;
        out = std::bit_cast<float>(v);
        return *this;
    }

    VectorReader& VectorReader::operator>>(double& out) {
        std::uint64_t v;
        *this >> v;
        out = std::bit_cast<double>(v);
        return *this;
    }

    VectorReader& VectorReader::operator>>(std::string& out) {
        std::uint32_t size;
        *this >> size;
        out.resize(size);
        for (std::size_t i = 0; i < size; ++i) {
            *this >> out[i];
        }
        return *this;
    }

    VectorReader& VectorReader::operator>>(std::u32string& out) {
        std::uint32_t size;
        *this >> size;
        out.resize(size);
        for (std::size_t i = 0; i < size; ++i) {
            std::uint32_t c;
            *this >> c;
            out[i] = std::bit_cast<char32_t>(c);
        }
        return *this;
    }

    VectorReader& VectorReader::operator>>(bool& out) {
        out = next() != 0;
        return *this;
    }
}