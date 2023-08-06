//
// Created by jglrxavpok on 06/08/2023.
//

#include <gtest/gtest.h>
#include <core/SparseArray.hpp>

using namespace Carrot;

TEST(SparseArrays, Fill) {
    SparseArray<int> array;
    EXPECT_EQ(array.size(), 0);
    EXPECT_EQ(array.sizeNonEmpty(), 0);

    array.resize(64);
    EXPECT_EQ(array.size(), 64);
    EXPECT_EQ(array.sizeNonEmpty(), 0);

    for (int i = 0; i < 32; ++i) {
        array[i] = i;
    }
    EXPECT_EQ(array.size(), 64);
    EXPECT_EQ(array.sizeNonEmpty(), 32);
    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(array[i], i);
    }

    for (int i = 32; i < 64; ++i) {
        array[i] = i;
    }
    EXPECT_EQ(array.size(), 64);
    EXPECT_EQ(array.sizeNonEmpty(), 64);

    for (int i = 0; i < 64; ++i) {
        EXPECT_EQ(array[i], i);
    }
}


TEST(SparseArrays, Resize) {
    struct Element {
        int v = -42;
    };

    SparseArray<Element> array;

    auto fill = [&]() {
        for(std::size_t i = 0; i < array.size(); i++) {
            array[i] = Element { (int)i };
        }
    };

    auto checkContents = [&]() {
        for(std::size_t i = 0; i < array.size(); i++) {
            EXPECT_EQ(array[i].v, (int)i);
        }
    };

    EXPECT_EQ(array.size(), 0);
    EXPECT_EQ(array.sizeNonEmpty(), 0);

    array.resize(64);
    fill();
    checkContents();

    EXPECT_EQ(array.size(), 64);
    EXPECT_EQ(array.sizeNonEmpty(), 64);

    // resize to smaller value
    array.resize(48);
    EXPECT_EQ(array.size(), 48);
    EXPECT_EQ(array.sizeNonEmpty(), 48);
    checkContents();
    EXPECT_FALSE(array.contains(48));
    EXPECT_THROW(array[48], std::out_of_range);

    // resize to bigger size
    array.resize(128);
    EXPECT_EQ(array.size(), 128);
    EXPECT_EQ(array.sizeNonEmpty(), 48); // old values are still gone
    EXPECT_FALSE(array.contains(48));
    EXPECT_NO_THROW(array[48]); // will create a new value
    EXPECT_EQ(array[48].v, -42); // not initialized
    fill();
    checkContents();
}

TEST(SparseArrays, Clear) {
    struct Element {
        int v = -42;
    };

    SparseArray<Element> array;

    auto fill = [&]() {
        for(std::size_t i = 0; i < array.size(); i++) {
            array[i] = Element { (int)i };
        }
    };

    auto checkContents = [&]() {
        for(std::size_t i = 0; i < array.size(); i++) {
            EXPECT_EQ(array[i].v, (int)i);
        }
    };

    EXPECT_EQ(array.size(), 0);
    EXPECT_EQ(array.sizeNonEmpty(), 0);

    array.resize(64);
    fill();
    checkContents();

    EXPECT_EQ(array.size(), 64);
    EXPECT_EQ(array.sizeNonEmpty(), 64);

    array.clear();
    EXPECT_EQ(array.size(), 64);
    EXPECT_EQ(array.sizeNonEmpty(), 0);
}