#include <gtest/gtest.h>

#include <core/async/Coroutines.hpp>

struct RefCountedObject {
    explicit RefCountedObject() {
        references++;
    }

    RefCountedObject(const RefCountedObject&) {
        references++;
    }

    RefCountedObject(RefCountedObject&&) {
        references++;
    }

    ~RefCountedObject() {
        references--;
    }

    static std::uint32_t references;
};

std::uint32_t RefCountedObject::references = 0;

Carrot::Async::Task<> voidTask(RefCountedObject obj) {
    co_return;
}

TEST(Coroutines, DestructorCalledOnScopeEnd) {
    {
        RefCountedObject obj{};
        auto task = voidTask(obj);
        ASSERT_EQ(RefCountedObject::references, 2); // original + move-constructor
        task();
    }
    ASSERT_EQ(RefCountedObject::references, 0);
}

TEST(Coroutines, TransferOwnership) {
    std::vector<Carrot::Async::Task<>> tasks;
    auto transferOwnership = [&](Carrot::Async::Task<>&& task) {
        tasks.emplace_back(std::move(task));
    };

    {
        RefCountedObject obj{};
        auto task = voidTask(obj);
        ASSERT_EQ(RefCountedObject::references, 2); // original + move-constructor
        transferOwnership(std::move(task));
    }
    // Task inside tasks vector
    ASSERT_EQ(RefCountedObject::references, 1);

    tasks.clear();

    ASSERT_EQ(RefCountedObject::references, 0);
}
