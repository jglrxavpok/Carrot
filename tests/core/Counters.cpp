//
// Created by jglrxavpok on 26/01/2022.
//
#include <gtest/gtest.h>
#include <future>
#include <core/async/Coroutines.hpp>
#include <core/async/Counter.h>

using namespace Carrot::Async;

TEST(Counters, SingleThreadedBehaviour) {
    Counter c;
    ASSERT_TRUE(c.isIdle());
    c.increment();
    ASSERT_TRUE(!c.isIdle());
    c.decrement();
    ASSERT_TRUE(c.isIdle());
}

Task<> someTask(Counter& counter) {
    counter.increment();
    co_await counter;
    co_return;
}

TEST(Counters, CoroutineWait) {
    Counter c;
    Task<> task = someTask(c);
    ASSERT_TRUE(c.isIdle());
    task();
    ASSERT_TRUE(!c.isIdle());
    ASSERT_FALSE(task.done());
    ASSERT_FALSE(task()); // task must not continue
    ASSERT_FALSE(task.done());
    c.decrement(); // task is continued here
    ASSERT_TRUE(task.done());
}

TEST(Counters, OnlyOneCoroutine) {
    Counter c;
    Task<> task1 = someTask(c);
    Task<> task2 = someTask(c);
    EXPECT_TRUE(c.isIdle());
    task1();
    EXPECT_THROW(task2(), Carrot::Assertions::Error);

    c.decrement();
    c.decrement();
}

TEST(Counters, BusyWait) {
    Counter c;
    c.increment();
    ASSERT_FALSE(c.isIdle());
    auto f = std::async(std::launch::async, [&]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        c.decrement();
    });
    ASSERT_FALSE(c.isIdle());
    c.busyWait(); // decrement in another thread
    ASSERT_TRUE(c.isIdle());
}

TEST(Counters, BusyWaitDuration) {
    Counter c;
    c.increment();
    ASSERT_FALSE(c.isIdle());
    auto f = std::async(std::launch::async, [&]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        c.decrement();
    });
    ASSERT_FALSE(c.isIdle());
    bool returned = false;
    returned = c.busyWaitFor(std::chrono::seconds(1)); // not yet decremented
    ASSERT_FALSE(c.isIdle());
    ASSERT_FALSE(returned);
    returned = c.busyWaitFor(std::chrono::seconds(1)); // not yet decremented
    ASSERT_FALSE(c.isIdle());
    ASSERT_FALSE(returned);
    returned = c.busyWaitFor(std::chrono::seconds(4)); // decremented in another thread
    ASSERT_TRUE(c.isIdle());
    ASSERT_TRUE(returned);
}

TEST(Counters, SleepWait) {
    Counter c;
    c.increment();
    ASSERT_FALSE(c.isIdle());
    auto f = std::async(std::launch::async, [&]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        c.decrement();
    });
    ASSERT_FALSE(c.isIdle());
    c.sleepWait(); // decrement in another thread
    ASSERT_TRUE(c.isIdle());
}