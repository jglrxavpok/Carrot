//
// Created by jglrxavpok on 01/05/2022.
//

#include <gtest/gtest.h>
#include <stdexcept>
#include <core/utils/stringmanip.h>

using namespace Carrot;

TEST(Strings, Replace) {
    std::string str = "My string";
    EXPECT_EQ(Carrot::replace(str, "ab", "bc"), "My string"); // no modifications (no such token)
    EXPECT_EQ(Carrot::replace(str, "my", "your"), "My string"); // no modifications (case sensitive)
    EXPECT_EQ(Carrot::replace(str, "My", "Your"), "Your string");

    std::string str2 = "mymymy string my your";
    EXPECT_EQ(Carrot::replace(str2, "my", "their"), "theirtheirtheir string their your"); // multiple replacements

    std::string str3 = "mymymy string";
    EXPECT_EQ(Carrot::replace(str3, "ymym", "aaa"), "maaay string"); // inner-word replacement
    EXPECT_EQ(Carrot::replace(str3, "ym", "ab"), "mababy string"); // inner-word multiple replacement
}
