//
// Created by jglrxavpok on 31/08/2021.
//

#pragma once

#include <vector>
#include <bit>
#include <cstdint>
#include <string>
#include <glm/glm.hpp>
#include <core/utils/Concepts.hpp>
#include <span>
#include <unordered_map>

// Writes are done in little-endian
namespace Carrot::IO {
    inline void write(std::vector<std::uint8_t>& destination, char v) {
        destination.push_back(std::bit_cast<std::uint8_t>(v));
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

    template<glm::length_t dim, typename Elem, glm::qualifier qualifier>
    inline void write(std::vector<std::uint8_t>& destination, glm::vec<dim, Elem, qualifier> v) {
        for (int i = 0; i < dim; ++i) {
            write(destination, (Elem)v[i]);
        }
    }

    template<typename Elem, glm::qualifier qualifier>
    inline void write(std::vector<std::uint8_t>& destination, glm::qua<Elem, qualifier> v) {
        for (int i = 0; i < 4; ++i) {
            write(destination, (Elem)v[i]);
        }
    }

    inline void write(std::vector<std::uint8_t>& destination, bool v) {
        write(destination, static_cast<std::uint8_t>(v ? 1u : 0u));
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
CarrotSerialiseOperator(bool)
#undef CarrotSerialiseOperator

template<glm::length_t dim, typename Elem, glm::qualifier qualifier>
inline std::vector<std::uint8_t>& operator<<(std::vector<std::uint8_t>& out, const glm::vec<dim, Elem, qualifier>& value) {
    Carrot::IO::write(out, value);
    return out;
}

template<typename Elem, glm::qualifier qualifier>
inline std::vector<std::uint8_t>& operator<<(std::vector<std::uint8_t>& out, const glm::qua<Elem, qualifier>& value) {
    Carrot::IO::write(out, value);
    return out;
}

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
        VectorReader& operator>>(bool& out);

        template<glm::length_t dim, typename Elem, glm::qualifier qualifier>
        VectorReader& operator>>(glm::vec<dim, Elem, qualifier>& value) {
            for (int i = 0; i < dim; ++i) {
                *this >> value[i];
            }
            return *this;
        }

        template<typename Elem, glm::qualifier qualifier>
        VectorReader& operator>>(glm::qua<Elem, qualifier>& value) {
            for (int i = 0; i < 4; ++i) {
                *this >> value[i];
            }
            return *this;
        }

        template<typename Elem, typename Alloc> requires Concepts::Deserializable<VectorReader, Elem>
        VectorReader& operator>>(std::vector<Elem, Alloc>& out) {
            std::size_t size;
            *this >> size;
            out.resize(size);
            for(std::size_t i = 0; i < size; i++) {
                *this >> out[i];
            }
            return *this;
        }

        template<typename Elem, std::size_t Size> requires Concepts::Deserializable<VectorReader, Elem>
        VectorReader& operator>>(std::span<Elem, Size> out) {
            for(std::size_t i = 0; i < out.size(); i++) {
                *this >> out[i];
            }
            return *this;
        }

        template<typename Elem, std::size_t Size> requires Concepts::Deserializable<VectorReader, Elem>
        VectorReader& operator>>(std::array<Elem, Size>& out) {
            for(std::size_t i = 0; i < out.size(); i++) {
                *this >> out[i];
            }
            return *this;
        }

        template<typename Key, typename Value, typename Hasher, typename EqualFunc, typename Alloc>
            requires Concepts::Deserializable<VectorReader, Key> && Concepts::Deserializable<VectorReader, Value>
        VectorReader& operator>>(std::unordered_map<Key, Value, Hasher, EqualFunc, Alloc>& map) {
            map.clear();
            std::size_t count;
            *this >> count;
            for(std::size_t i = 0; i < count; i++) {
                Key k;
                Value v;
                *this >> k;
                *this >> v;
                map[std::move(k)] = std::move(v);
            }
            return *this;
        }

        template<typename Elem, std::size_t N>
            requires Concepts::Deserializable<VectorReader, Elem>
        VectorReader& operator>>(Elem map[N]) {
            for(std::size_t i = 0; i < N; i++) {
                *this >> map[i];
            }
            return *this;
        }

    private:
        std::uint8_t next();

        const std::vector<std::uint8_t>& data;
        std::size_t ptr = 0;
    };

    /// Allows to write data to a std::vector, with Carrot::IO::write methods. Little-endian is used for both
    class VectorWriter {
    public:
        explicit VectorWriter(std::vector<std::uint8_t>& vector): data(vector) {}
        ~VectorWriter() = default;

        VectorWriter& operator<<(const char& input);
        VectorWriter& operator<<(const std::uint8_t& input);
        VectorWriter& operator<<(const std::uint16_t& input);
        VectorWriter& operator<<(const std::uint32_t& input);
        VectorWriter& operator<<(const std::uint64_t& input);
        VectorWriter& operator<<(const float& input);
        VectorWriter& operator<<(const double& input);
        VectorWriter& operator<<(const std::u32string& input);

        VectorWriter& operator<<(const std::string& input);
        VectorWriter& operator<<(const bool& input);

        template<glm::length_t dim, typename Elem, glm::qualifier qualifier>
        VectorWriter& operator<<(const glm::vec<dim, Elem, qualifier>& value) {
            for (int i = 0; i < dim; ++i) {
                *this << value[i];
            }
            return *this;
        }

        template<typename Elem, glm::qualifier qualifier>
        VectorWriter& operator<<(const glm::qua<Elem, qualifier>& value) {
            for (int i = 0; i < 4; ++i) {
                *this << value[i];
            }
            return *this;
        }

        template<typename Elem, size_t Size> requires Concepts::Serializable<VectorWriter, Elem>
        VectorWriter& operator<<(const std::span<Elem, Size>& span) {
            for(const auto& e : span) {
                *this << e;
            }
            return *this;
        }

        template<typename Elem, size_t Size> requires Concepts::Serializable<VectorWriter, Elem>
        VectorWriter& operator<<(const std::array<Elem, Size>& span) {
            for(const auto& e : span) {
                *this << e;
            }
            return *this;
        }

        template<typename Elem> requires Concepts::Serializable<VectorWriter, Elem>
        VectorWriter& operator<<(const std::vector<Elem>& v) {
            *this << v.size();
            for(const auto& e : v) {
                *this << e;
            }
            return *this;
        }

        template<typename Key, typename Value, typename Hasher, typename EqualFunc, typename Alloc>
            requires Concepts::Serializable<VectorWriter, Key> && Concepts::Serializable<VectorWriter, Value>
        VectorWriter& operator<<(const std::unordered_map<Key, Value, Hasher, EqualFunc, Alloc>& map) {
            *this << map.size();
            for(const auto& [k, v] : map) {
                *this << k;
                *this << v;
            }
            return *this;
        }

        template<typename Elem, std::size_t N>
            requires Concepts::Serializable<VectorWriter, Elem>
        VectorWriter& operator<<(const Elem map[N]) {
            for(std::size_t i = 0; i < N; i++) {
                *this << map[i];
            }
            return *this;
        }

    private:
        std::uint8_t& next();
        void grow(std::size_t size);

        std::vector<std::uint8_t>& data;
        std::size_t ptr = 0;
    };
}