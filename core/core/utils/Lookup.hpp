//
// Created by jglrxavpok on 12/01/2022.
//

#pragma once

#include <array>
#include <stdexcept>

namespace Carrot {
    namespace {
        using Str = const char*;
    }

    template<typename T>
    struct LookupEntry {
        T value;
        Str name;
    };

    template<typename T, std::size_t Size>
    class Lookup {
    public:
        constexpr Lookup(std::array<LookupEntry<T>, Size> list) {
            for (std::size_t i = 0; i < Size; ++i) {
                storage[i] = list[i];
            }
        }

    public:
        [[nodiscard]] constexpr std::size_t size() const {
            return Size;
        }

    public:
        constexpr const char* at(const T& v) const {
            for(const auto& entry : storage) {
                if(v == entry.value) {
                    return entry.name;
                }
            }
            throw std::invalid_argument("Unknown value");
        }

        constexpr const char* operator[](const T& v) const {
            return at(v);
        }

    public:
        constexpr T at(const std::string& str) const {
            for(const auto& entry : storage) {
                if(str == entry.name) {
                    return entry.value;
                }
            }
            throw std::invalid_argument(str);
        }

        constexpr T operator[](const std::string& str) const {
            return at(str);
        }

        constexpr const LookupEntry<T>* begin() const {
            static_assert(Size > 0);
            return &storage[0];
        }

        constexpr const LookupEntry<T>* end() const {
            static_assert(Size > 0);
            return &storage[Size-1];
        }

    private:
        LookupEntry<T> storage[Size];
    };
}