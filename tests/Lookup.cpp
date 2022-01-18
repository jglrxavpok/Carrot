//
// Created by jglrxavpok on 12/01/2022.
//

#include <gtest/gtest.h>

#include <stdexcept>
#include <array>
#include <core/utils/Lookup.hpp>

using namespace Carrot;

TEST(Lookup, GetExistingFromValue) {
    Lookup table = std::array {
            LookupEntry<std::uint32_t> { 42, "Answer to life, the universe and everything" },
            LookupEntry<std::uint32_t> { 0xCAFEBABE, "Java magic number" },
            LookupEntry<std::uint32_t> { 4, "Random dice roll" },
    };

    ASSERT_EQ(table.size(), 3);
    ASSERT_STREQ("Answer to life, the universe and everything", table[42]);
    ASSERT_STREQ("Random dice roll", table.at(4));
}

TEST(Lookup, FailToGetFromValue) {
    Lookup table = std::array {
            LookupEntry<std::uint32_t> { 4, "Random dice roll" },
    };

    ASSERT_EQ(table.size(), 1);
    ASSERT_THROW(table[22], std::invalid_argument);
    ASSERT_THROW(table.at(14), std::invalid_argument);
}

TEST(Lookup, GetExistingFromStr) {
    Lookup table = std::array {
            LookupEntry<std::uint32_t> { 42, "Answer to life, the universe and everything" },
            LookupEntry<std::uint32_t> { 0xCAFEBABE, "Java magic number" },
            LookupEntry<std::uint32_t> { 4, "Random dice roll" },
    };

    ASSERT_EQ(table.size(), 3);
    ASSERT_EQ(42, table["Answer to life, the universe and everything"]);
    ASSERT_EQ(4, table.at("Random dice roll"));
}

TEST(Lookup, FailToGetFromStr) {
    Lookup table = std::array {
            LookupEntry<std::uint32_t> { 42, "Answer to life, the universe and everything" },
    };

    ASSERT_EQ(table.size(), 1);
    ASSERT_THROW(table["Some stuff"], std::invalid_argument);
    ASSERT_THROW(table.at("Some stuff"), std::invalid_argument);
}