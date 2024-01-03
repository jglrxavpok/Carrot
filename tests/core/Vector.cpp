//
// Created by jglrxavpok on 02/01/2024.
//

#include <core/UniquePtr.hpp>
#include <core/allocators/StackAllocator.h>
#include <core/containers/Vector.hpp>
#include <gtest/gtest.h>

using namespace Carrot;

static std::atomic<std::int64_t> StructCount{0};
static std::atomic<std::int64_t> ConstructorCalls{0};

struct S2 {
    int a = 42;

    S2() {
        ++StructCount;
        ++ConstructorCalls;
    };

    explicit S2(int val): S2() {
        a = val;
    };

    S2(const S2& o): S2() {
        a = o.a;
    }

    explicit S2(S2&& o): S2() {
        a = o.a;
    }

    S2& operator=(const S2& o) {
        a = o.a;
        return *this;
    }

    S2& operator=(S2&& o) {
        a = o.a;
        return *this;
    }

    bool operator==(const S2& o) const {
        return a == o.a;
    }

    bool operator!=(const S2& o) const {
        return a != o.a;
    }

    ~S2() {
        --StructCount;
    }
};

TEST(Vector, InitMustBeEmpty) {
    ConstructorCalls = 0;
    Vector<S2> v { MallocAllocator::instance };
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 0);
}

TEST(Vector, PushBackAndEmplaceBack) {
    ConstructorCalls = 0;
    Vector<S2> v { MallocAllocator::instance };
    EXPECT_EQ(v.size(), 0);

    {
        v.pushBack(S2{});
    }
    EXPECT_EQ(v[0].a, 42);
    EXPECT_EQ(v.size(), 1);
    EXPECT_EQ(StructCount, 1);
    EXPECT_EQ(ConstructorCalls, 2); // construction + copy inside pushBack

    S2& newElement = v.emplaceBack(50);
    EXPECT_EQ(v[0].a, 42); // first element must not be modified
    EXPECT_EQ(v.size(), 2);
    EXPECT_EQ(newElement.a, 50);
    EXPECT_EQ(StructCount, 2);
    EXPECT_EQ(ConstructorCalls, 3); // only a single additional call after emplaceBack
}

TEST(Vector, Remove) {
    ConstructorCalls = 0;
    Vector<S2> v { MallocAllocator::instance };
    EXPECT_EQ(v.size(), 0);

    v.pushBack(S2{});
    EXPECT_EQ(v[0].a, 42);
    EXPECT_EQ(v.size(), 1);
    EXPECT_EQ(StructCount, 1);

    {
        S2& newElement = v.emplaceBack(50);
        EXPECT_EQ(v[0].a, 42); // first element must not be modified
        EXPECT_EQ(v.size(), 2);
        EXPECT_EQ(newElement.a, 50);
        EXPECT_EQ(StructCount, 2);
    }

    v.remove(0);
    EXPECT_EQ(StructCount, 1);
    EXPECT_EQ(v[0].a, 50);
    EXPECT_EQ(v.size(), 1);
}

TEST(Vector, DestructionDeletesEverything) {
    ConstructorCalls = 0;
    {
        Vector<S2> v { MallocAllocator::instance };
        v.pushBack(S2{});
        v.emplaceBack(50);
        EXPECT_EQ(StructCount, 2);
        EXPECT_EQ(v.size(), 2);
    }
    EXPECT_EQ(StructCount, 0);
}

TEST(Vector, Iteration) {
    ConstructorCalls = 0;
    int elementCount = 100;
    Vector<S2> v { MallocAllocator::instance };

    v.resize(elementCount);
    for (int i = 0; i < elementCount; i++) {
        v[i].a = i;
    }

    // forward
    std::size_t index = 0;
    for(S2& s : v) {
        EXPECT_EQ(s.a, index);
        index++;
    }

    index = 0;
    const auto& vConst = v;
    for(const S2& s : vConst) {
        EXPECT_EQ(s.a, index);
        index++;
    }

    // backwards
    index = v.size();
    for(auto it = v.rbegin(); it != v.rend(); ++it) {
        index--;
        EXPECT_EQ(it->a, index);
    }
}

TEST(Vector, Copy) {
    ConstructorCalls = 0;
    auto createVector = [&](int elementCount)
    {
        Vector<S2> v { MallocAllocator::instance };
        v.resize(elementCount);
        for (int i = 0; i < elementCount; i++) {
            v[i].a = i;
        }
        return v;
    };

    Vector<S2> v1 = createVector(10);
    Vector<S2> v2 = createVector(25);

    v1 = v2;
    EXPECT_EQ(v1.size(), v2.size());
    EXPECT_EQ(v2.size(), 25);
    EXPECT_EQ(v1, v2);
    EXPECT_EQ(StructCount, 25*2);
}

TEST(Vector, Move) {
    ConstructorCalls = 0;
    auto createVector = [&](Allocator& allocator, int elementCount)
    {
        Vector<S2> v { allocator };
        v.resize(elementCount);
        for (int i = 0; i < elementCount; i++) {
            v[i].a = i;
        }
        return v;
    };

    // compatible allocators
    {
        Vector<S2> v1 = createVector(MallocAllocator::instance, 10);
        Vector<S2> v2 = createVector(MallocAllocator::instance, 25);
        EXPECT_TRUE(v1.getAllocator().isCompatibleWith(v2.getAllocator()));

        EXPECT_EQ(ConstructorCalls, 35);

        S2* originalV2Data = v2.data();
        const std::size_t originalV2Capacity = v2.capacity();
        v1 = std::move(v2);

        // check contents
        EXPECT_EQ(v1.size(), 25);
        EXPECT_EQ(v2.size(), 0);
        for (int i = 0; i < v1.size(); ++i) {
            EXPECT_EQ(v1[i].a, i);
        }

        // check that memory was moved from one container to the other
        EXPECT_EQ(v1.data(), originalV2Data);
        EXPECT_EQ(v1.capacity(), originalV2Capacity);
        EXPECT_EQ(v2.data(), nullptr);
        EXPECT_EQ(v2.capacity(), 0); // entire memory was moved

        EXPECT_EQ(StructCount, 25);
        EXPECT_EQ(ConstructorCalls, 35); // no move constructor needed!
    }

    ConstructorCalls = 0;

    // incompatible allocators
    {
        Vector<S2> v1 = createVector(MallocAllocator::instance, 10);

        StackAllocator stackAllocator { MallocAllocator::instance };
        Vector<S2> v2 = createVector(stackAllocator, 25);
        EXPECT_FALSE(v1.getAllocator().isCompatibleWith(v2.getAllocator()));

        EXPECT_EQ(ConstructorCalls, 35);

        S2* originalV2Data = v2.data();
        const std::size_t originalV2Capacity = v2.capacity();
        v1 = std::move(v2);

        // check contents
        EXPECT_EQ(v1.size(), 25);
        EXPECT_EQ(v2.size(), 0);
        for (int i = 0; i < v1.size(); ++i) {
            EXPECT_EQ(v1[i].a, i);
        }

        // check that memory was not moved from one container to the other
        EXPECT_NE(v1.data(), originalV2Data); // needed to do a new allocation
        EXPECT_EQ(v2.data(), nullptr);

        EXPECT_EQ(ConstructorCalls, 35 + 25); // construction of vectors + move constructors
        EXPECT_EQ(StructCount, 25); // make sure the structs inside v1 were properly deleted
    }
}