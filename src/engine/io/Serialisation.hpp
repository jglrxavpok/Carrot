//
// Created by jglrxavpok on 31/08/2021.
//

#pragma once

namespace Carrot::IO {
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
#undef CarrotSerialiseOperator

namespace Carrot::IO {
    class VectorReader {
    public:
        explicit VectorReader(const std::vector<char>& vector): data(vector) {}
        ~VectorReader() = default;

    private:
        const std::vector<char>& data;
        std::size_t ptr = 0;
    };
}