//
// Created by jglrxavpok on 26/01/2022.
//
#include <gtest/gtest.h>
#include <future>
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