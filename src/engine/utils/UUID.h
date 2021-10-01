//
// Created by jglrxavpok on 07/05/2021.
//

#pragma once

#include <uuid.h>
#include <xhash>

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
        explicit UUID() {
            Generator generator;
            uuid = generator();
        }

        UUID(const UUID&) = default;
        UUID(UUID&&) = default;

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

        static UUID fromString(std::string_view str) {
            UUID value;
            value.uuid = uuids::uuid::from_string(str).value();
            return value;
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