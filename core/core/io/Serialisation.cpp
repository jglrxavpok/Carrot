//
// Created by jglrxavpok on 02/09/2021.
//

#include "Serialisation.h"
#include <core/Macros.h>

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

    VectorWriter& VectorWriter::operator<<(const char& input) {
        grow(1);
        next() = std::bit_cast<std::uint8_t>(input);
        return *this;
    }

    VectorWriter& VectorWriter::operator<<(const std::uint8_t& input) {
        grow(1);
        next() = input;
        return *this;
    }

    VectorWriter& VectorWriter::operator<<(const std::uint16_t& input) {
        grow(2);
        *this << static_cast<std::uint8_t>(input & 0xFF);
        *this << static_cast<std::uint8_t>((input >> 8) & 0xFF);
        return *this;
    }

    VectorWriter& VectorWriter::operator<<(const std::uint32_t& input) {
        grow(4);
        *this << static_cast<std::uint8_t>(input & 0xFF);
        *this << static_cast<std::uint8_t>((input >> 8) & 0xFF);
        *this << static_cast<std::uint8_t>((input >> 16) & 0xFF);
        *this << static_cast<std::uint8_t>((input >> 24) & 0xFF);
        return *this;
    }

    VectorWriter& VectorWriter::operator<<(const std::uint64_t& input) {
        grow(8);
        *this << static_cast<std::uint32_t>(input & 0xFFFFFFFF);
        *this << static_cast<std::uint32_t>((input >> 32) & 0xFFFFFFFF);
        return *this;
    }

    VectorWriter& VectorWriter::operator<<(const float& input) {
        *this << std::bit_cast<std::uint32_t>(input);
        return *this;
    }

    VectorWriter& VectorWriter::operator<<(const double& input) {
        *this << std::bit_cast<std::uint64_t>(input);
        return *this;
    }

    VectorWriter& VectorWriter::operator<<(const std::u32string& input) {
        std::size_t s = input.size();
        grow(s * sizeof(std::uint32_t) + sizeof(std::size_t));
        *this << s;
        for(const auto& element : input) {
            std::uint32_t c = std::bit_cast<std::uint32_t>(element);
            *this << c;
        }
        return *this;
    }

    VectorWriter& VectorWriter::operator<<(const std::string& input) {
        std::size_t s = input.size();
        grow(s + sizeof(std::size_t));
        *this << s;
        for(const auto& c : input) {
            *this << c;
        }
        return *this;
    }

    VectorWriter& VectorWriter::operator<<(const bool& input) {
        grow(1);
        next() = input ? 1 : 0;
        return *this;
    }

    std::uint8_t& VectorWriter::next() {
        verify(ptr < data.size(), "Writing past end of buffer, missing a 'grow' call?");
        return data[ptr++];
    }

    void VectorWriter::grow(std::size_t size) {
        if(ptr + size >= data.size()) {
            data.resize(ptr+size);
        }
    }
}