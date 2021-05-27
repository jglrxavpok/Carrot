//
// Created by jglrxavpok on 07/05/2021.
//

#pragma once

#include <uuid.h>

namespace Carrot {

    using UUID = uuids::uuid;

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

    inline UUID randomUUID() {
        Generator generator;
        return generator();
    }

    inline UUID fromString(const std::string& str) {
        return uuids::uuid::from_string(str).value();
    }

    inline std::string toString(const UUID& uuid) {
        return uuids::to_string(uuid);
    }
}