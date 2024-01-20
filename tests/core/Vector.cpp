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

TEST(Vector, Span) {
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

    Vector<S2> v = createVector(100);
    std::span<S2> asSpan = v; // Vector must convert to a span easily
    EXPECT_EQ(v.size(), asSpan.size());
    EXPECT_EQ(v.data(), asSpan.data());

    const Vector<S2> v2 = createVector(100);
    std::span<const S2> asSpan2 = v2; // Vector must convert to a span easily
    EXPECT_EQ(v2.size(), asSpan2.size());
    EXPECT_EQ(v2.cdata(), asSpan2.data());
}

TEST(Vector, IteratorOperations) {
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

    Vector<S2> emptyVector;
    EXPECT_EQ(emptyVector.begin(), emptyVector.end());

    Vector<S2> v = createVector(100);
    auto begin = v.begin();
    auto end = v.end();
    EXPECT_NE(begin, end);
    EXPECT_EQ(begin+v.size(), end);
    EXPECT_EQ(end-begin, v.size());
    EXPECT_EQ(begin-end, -(static_cast<std::int64_t>(v.size())));
    EXPECT_EQ(end-end, 0);
    EXPECT_EQ(begin-begin, 0);
    EXPECT_EQ(end - static_cast<std::ptrdiff_t>(v.size()), begin);

    auto next = begin;
    next = ++next;
    EXPECT_EQ(next->a, v[1].a);
    EXPECT_EQ(next-begin, 1);
    EXPECT_EQ(begin-next, -1);
    EXPECT_EQ(end-next, v.size()-1);
    EXPECT_EQ(next-end, -(static_cast<std::int64_t>(v.size()-1)));
    EXPECT_EQ(begin+1, next);

    auto nextNext = next;
    ++nextNext;
    EXPECT_EQ(nextNext->a, v[2].a);
    EXPECT_EQ(nextNext-begin, 2);
    EXPECT_EQ(begin-nextNext, -2);
    EXPECT_EQ(end-nextNext, v.size()-2);
    EXPECT_EQ(nextNext-end, -(static_cast<std::int64_t>(v.size()-2)));
    EXPECT_EQ(nextNext-next, 1);
    EXPECT_EQ(next+1, nextNext);
    EXPECT_EQ(begin+2, nextNext);

    auto rbegin = v.rbegin();
    auto rend = v.rend();
    EXPECT_NE(rbegin, rend);
    EXPECT_EQ(rbegin+v.size(), rend);
    EXPECT_EQ(rend-rbegin, v.size());
    EXPECT_EQ(rbegin-rend, -(static_cast<std::int64_t>(v.size())));
    EXPECT_EQ(rend-rend, 0);
    EXPECT_EQ(rbegin-rbegin, 0);

    auto rnext = rbegin;
    ++rnext;
    EXPECT_EQ(rnext->a, v[v.size()-2].a);
    EXPECT_EQ(rnext-rbegin, 1);
    EXPECT_EQ(rbegin-rnext, -1);
    EXPECT_EQ(rend-rnext, v.size()-1);
    EXPECT_EQ(rnext-rend, -(static_cast<std::int64_t>(v.size()-1)));
    EXPECT_EQ(rbegin+1, rnext);

    auto rnextNext = rnext;
    ++rnextNext;
    EXPECT_EQ(rnextNext->a, v[v.size()-3].a);
    EXPECT_EQ(rnextNext-rbegin, 2);
    EXPECT_EQ(rbegin-rnextNext, -2);
    EXPECT_EQ(rend-rnextNext, v.size()-2);
    EXPECT_EQ(rnextNext-rend, -(static_cast<std::int64_t>(v.size()-2)));
    EXPECT_EQ(rnextNext-rnext, 1);
    EXPECT_EQ(rnext+1, rnextNext);
    EXPECT_EQ(rbegin+2, rnextNext);

    for (std::size_t i = 0; i < v.size(); i++) {
        EXPECT_EQ((begin+i)->a, v[i].a);
        EXPECT_EQ((end-i-1)->a, v[v.size()-i-1].a);
    }

    for (std::size_t i = 0; i < v.size(); i++) {
        EXPECT_EQ((rbegin+i)->a, v[v.size()-i-1].a);
        EXPECT_EQ((rend-i-1)->a, v[i].a);
    }
}

TEST(Vector, Sort) {
    ConstructorCalls = 0;
    auto createVector = [&](int elementCount)
    {
        Vector<S2> v { MallocAllocator::instance };
        v.resize(elementCount);
        for (int i = 0; i < elementCount; i++) {
            v[i].a = elementCount-i;
        }
        return v;
    };

    Vector<S2> v = createVector(10);
    auto compareOp = [&](const S2& a, const S2& b) {
        return a.a < b.a;
    };

    v.sort(compareOp);
    for (int i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i].a, i+1);
    }

    // sort in reverse order
    // to make sure reverse iterators are implemented properly
    std::sort(v.rbegin(), v.rend(), compareOp);
    for (int i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v[i].a, v.size()-i);
    }
}

TEST(Vector, GrowthFactor) {
    {
        // on pushBack, if the size == capacity, set capacity to newSize * growthFactor
        Carrot::Vector<float> v;
        v.setGrowthFactor(2);

        EXPECT_EQ(v.capacity(), 0);

        v.pushBack(0.0f);
        EXPECT_EQ(v.size(), 1);
        EXPECT_EQ(v.capacity(), 2);

        v.pushBack(0.0f);
        EXPECT_EQ(v.size(), 2);
        EXPECT_EQ(v.capacity(), 2);

        v.pushBack(0.0f);
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v.capacity(), 6);
    }

    {
        // on emplaceBack, if the size == capacity, set capacity to newSize * growthFactor
        Carrot::Vector<float> v;
        v.setGrowthFactor(2);

        EXPECT_EQ(v.capacity(), 0);

        v.emplaceBack(0.0f);
        EXPECT_EQ(v.size(), 1);
        EXPECT_EQ(v.capacity(), 2);

        v.emplaceBack(0.0f);
        EXPECT_EQ(v.size(), 2);
        EXPECT_EQ(v.capacity(), 2);

        v.emplaceBack(0.0f);
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v.capacity(), 6);
    }

    {
        // on emplaceBack, if the size == capacity, set capacity to newSize * growthFactor
        Carrot::Vector<float> v;
        v.setGrowthFactor(1.5f); // non-integer factors will use ceil

        EXPECT_EQ(v.capacity(), 0);

        v.emplaceBack(0.0f);
        EXPECT_EQ(v.size(), 1);
        EXPECT_EQ(v.capacity(), 2);

        v.emplaceBack(0.0f);
        EXPECT_EQ(v.size(), 2);
        EXPECT_EQ(v.capacity(), 2);

        v.emplaceBack(0.0f);
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v.capacity(), 5); // 3 * 1.5 = 4.5, so 5 (ceil)
    }
}