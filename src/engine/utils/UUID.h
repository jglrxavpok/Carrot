//
// Created by jglrxavpok on 07/05/2021.
//

#pragma once

#include <uuid.h>
#include <xhash>
#include "engine/utils/Macros.h"

namespace Carrot {

    namespace {
        struct Generator {
            uuids::uuid_random_generator* generator;

            Generator() {
                std::random_device rd;
                auto seed_data = std::array<int, std::mt19937::state_size> {};
                std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
                std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
                std::mt19937 engine(seq);
                generator = new uuids::uuid_random_generator(&engine);
            }

            uuids::uuid operator()() {
                return (*generator)();
            }

            ~Generator() {
                delete generator;
            }
        };
    }

    class UUID {
    public:
        static const UUID& null();

        explicit UUID() {
            Generator generator;
            uuid = generator();
        }

        UUID(std::uint32_t data0, std::uint32_t data1, std::uint32_t data2, std::uint32_t data3) {
            std::uint32_t data[4] { data0, data1, data2, data3 };
            std::span<std::uint8_t, 16> dataView = std::span<std::uint8_t, 16> { reinterpret_cast<std::uint8_t*>(data), static_cast<std::size_t>(16) };
            uuid = uuids::uuid(dataView);
        }

        UUID(const UUID&) = default;
        UUID(UUID&&) = default;

        UUID(const std::string& str) {
            uuid = uuids::uuid::from_string(str).value();
        }

        UUID(const char* str) {
            uuid = uuids::uuid::from_string(str).value();
        }

        UUID& operator=(const UUID&) = default;
        UUID& operator=(UUID&&) = default;

        bool operator==(const UUID& other) const {
            return uuid == other.uuid;
        }

        [[nodiscard]] std::string toString() const {
            return uuids::to_string(uuid);
        }

        [[nodiscard]] std::size_t hash() const {
            // Java style hashing

            const std::size_t prime = 31;

            auto asbytes = uuid.as_bytes();
            std::size_t hashResult = 0;
            for (auto byte : asbytes) {
                hashResult = static_cast<std::size_t>(byte) + (hashResult * prime);
            }
            return hashResult;
        }

        [[nodiscard]] std::uint32_t data0() const {
            return data(0);
        }

        [[nodiscard]] std::uint32_t data1() const {
            return data(1);
        }

        [[nodiscard]] std::uint32_t data2() const {
            return data(2);
        }

        [[nodiscard]] std::uint32_t data3() const {
            return data(3);
        }

        [[nodiscard]] std::uint32_t data(std::uint8_t index) const {
            verify(index == 0 || index == 1 || index == 2 || index == 3, "Invalid index");
            auto bytes = uuid.as_bytes();
            std::uint32_t result = 0;
            for (int i = 0; i < 4; ++i) {
                result |= static_cast<std::uint32_t>(bytes[i + index*4]) << (i*8);
            }
            return result;
        }

        static UUID fromString(std::string_view str) {
            return UUID(std::string(str));
        }

    private:
        uuids::uuid uuid;
    };
}

namespace std {
    template<>
    struct hash<Carrot::UUID> {
        std::size_t operator()(const Carrot::UUID& uuid) const {
            return uuid.hash();
        }
    };
}