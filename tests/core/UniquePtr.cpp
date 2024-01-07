//
// Created by jglrxavpok on 02/01/2024.
//

#include <core/UniquePtr.hpp>
#include <gtest/gtest.h>

using namespace Carrot;

static std::atomic<std::int64_t> StructCount{0};

struct S {
    int a = 42;

    S() {
        ++StructCount;
    };

    explicit S(int val): S() {
        a = val;
    };

    ~S() {
        --StructCount;
    }
};

TEST(UniquePtr, InitMustBeEmpty) {
    UniquePtr<S> p;
    EXPECT_EQ(p, nullptr);
    EXPECT_EQ(p.get(), nullptr);

    UniquePtr<S> p2 = nullptr;
    EXPECT_EQ(p2, nullptr);
    EXPECT_EQ(p2.get(), nullptr);
}

TEST(UniquePtr, MakeUniqueCallsConstructor) {
    {
        UniquePtr<S> p = makeUnique<S>(MallocAllocator::instance);
        EXPECT_EQ(p->a, 42);

        EXPECT_EQ(StructCount, 1);

        {
            UniquePtr<S> p2 = makeUnique<S>(MallocAllocator::instance, 50);
            EXPECT_EQ(p2->a, 50);

            EXPECT_EQ(StructCount, 2);
        } // p2 goes out of scope, deleting struct
        EXPECT_EQ(StructCount, 1);
    }
    EXPECT_EQ(StructCount, 0);
}

TEST(UniquePtr, MoveOperations) {
    UniquePtr<S> p1 = makeUnique<S>(MallocAllocator::instance);
    EXPECT_EQ(StructCount, 1);
    EXPECT_EQ(p1->a, 42);

    UniquePtr<S> p2 = std::move(p1);
    EXPECT_EQ(StructCount, 1);
    EXPECT_EQ(p1, nullptr); // NOLINT(*-use-after-move)
    EXPECT_NE(p2, nullptr);
    EXPECT_EQ(p2->a, 42);

    p2 = nullptr;
    EXPECT_EQ(p2, nullptr);
    EXPECT_EQ(StructCount, 0);

    p2 = makeUnique<S>(MallocAllocator::instance, 50);
    EXPECT_EQ(p2->a, 50);
    EXPECT_EQ(StructCount, 1);

    UniquePtr<S> emptyPtr;
    p2 = std::move(emptyPtr);
    EXPECT_EQ(StructCount, 0);
    EXPECT_EQ(p2, nullptr);
}
