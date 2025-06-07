//
// Created by jglrxavpok on 07/06/25.
//

#pragma once

#include <string_view>

namespace Carrot {

    // Basically a std::string with a precomputed CRC. Used for quick hashing and lookup in hashmaps
    class Identifier {
    public:
        Identifier();
        explicit Identifier(std::string_view contents);
        Identifier(Identifier&&) noexcept ;
        Identifier(const Identifier&);

        Identifier& operator=(const Identifier&);
        Identifier& operator=(Identifier&&) noexcept ;

        [[nodiscard]] std::size_t getHash() const;
        operator std::string_view() const; // NOLINT(*-explicit-constructor)

        bool operator==(const Identifier&) const;

    private:
        void update(std::string_view text);

        std::string storage;
        u64 crc = 0;
    };

} // Carrot

template<>
struct std::hash<Carrot::Identifier> {
    std::size_t operator()(const Carrot::Identifier& identifier) const noexcept;
};
