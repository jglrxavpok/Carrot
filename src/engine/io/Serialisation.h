//
// Created by jglrxavpok on 31/08/2021.
//

#pragma once

#include <vector>
#include <cstdint>
#include <string>

// Writes are done in little-endian
namespace Carrot::IO {
    inline void write(std::vector<std::uint8_t>& destination, char v) {
        write(destination, std::bit_cast<std::uint8_t>(v));
    }

    inline void write(std::vector<std::uint8_t>& destination, std::uint8_t v) {
        destination.push_back(v);
    }

    inline void write(std::vector<std::uint8_t>& destination, std::uint16_t v) {
        destination.push_back(v & 0xFF);
        destination.push_back((v >> 8) & 0xFF);
    }

    inline void write(std::vector<std::uint8_t>& destination, std::uint32_t v) {
        destination.push_back(v & 0xFF);
        destination.push_back((v >> 8) & 0xFF);
        destination.push_back((v >> 16) & 0xFF);
        destination.push_back((v >> 24) & 0xFF);
    }

    inline void write(std::vector<std::uint8_t>& destination, std::uint64_t v) {
        destination.push_back(v & 0xFF);
        destination.push_back((v >> 8) & 0xFF);
        destination.push_back((v >> 16) & 0xFF);
        destination.push_back((v >> 24) & 0xFF);
        destination.push_back((v >> 32) & 0xFF);
        destination.push_back((v >> 48) & 0xFF);
        destination.push_back((v >> 56) & 0xFF);
        destination.push_back((v >> 60) & 0xFF);
    }

    inline void write(std::vector<std::uint8_t>& destination, float v) {
        write(destination, std::bit_cast<std::uint32_t>(v));
    }

    inline void write(std::vector<std::uint8_t>& destination, double v) {
        write(destination, std::bit_cast<std::uint64_t>(v));
    }

    inline void write(std::vector<std::uint8_t>& destination, std::string_view str) {
        write(destination, static_cast<std::uint32_t>(str.size()));
        for(const auto& v : str) {
            write(destination, std::bit_cast<std::uint8_t>(v));
        }
    }

    inline void write(std::vector<std::uint8_t>& destination, std::u32string_view str) {
        write(destination, static_cast<std::uint32_t>(str.size()));
        for(const auto& v : str) {
            write(destination, std::bit_cast<std::uint32_t>(v));
        }
    }
}

#define CarrotSerialiseOperator(Type) \
    inline std::vector<std::uint8_t>& operator<<(std::vector<std::uint8_t>& out, Type value) { \
        Carrot::IO::write(out, value);\
        return out;                   \
    }

CarrotSerialiseOperator(std::uint8_t)
CarrotSerialiseOperator(std::uint16_t)
CarrotSerialiseOperator(std::uint32_t)
CarrotSerialiseOperator(std::uint64_t)
CarrotSerialiseOperator(float)
CarrotSerialiseOperator(double)
CarrotSerialiseOperator(std::string_view)
CarrotSerialiseOperator(std::u32string_view)
CarrotSerialiseOperator(char)
#undef CarrotSerialiseOperator

namespace Carrot::IO {
    /// Allows to read data written to a std::vector, with Carrot::IO::write methods. Little-endian is used for both
    class VectorReader {
    public:
        explicit VectorReader(const std::vector<std::uint8_t>& vector): data(vector) {}
        ~VectorReader() = default;

        VectorReader& operator>>(char& out);
        VectorReader& operator>>(std::uint8_t& out);
        VectorReader& operator>>(std::uint16_t& out);
        VectorReader& operator>>(std::uint32_t& out);
        VectorReader& operator>>(std::uint64_t& out);
        VectorReader& operator>>(float& out);
        VectorReader& operator>>(double& out);
        VectorReader& operator>>(std::u32string& out);

        VectorReader& operator>>(std::string& out);

    private:
        std::uint8_t next();

        const std::vector<std::uint8_t>& data;
        std::size_t ptr = 0;
    };
}