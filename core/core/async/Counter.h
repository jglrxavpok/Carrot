//
// Created by jglrxavpok on 26/01/2022.
//

#pragma once

#include <atomic>
#include <coroutine>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <source_location>
#include <core/Macros.h>
#include <cider/WaitQueue.h>
#include <cider/Fiber.h>
#include "Locks.h"

namespace Carrot::Async {
    template<class T>
    struct CoroutinePromiseType;

    class Counter;
    class TaskLane;

    struct CounterLanePair {
        Counter& counter;
        const TaskLane& lane;
    };

    /// Counter to determine when tasks are done
    class Counter {
    public:
        Counter(std::source_location source = std::source_location::current()): source(source) {};
        ~Counter();

        Counter(Counter&&);
        Counter(const Counter&) = delete;

        /// Waits for the counter to reach 0 in a while-loop and with thread yield
        void busyWait();

        /// Waits for the counter to reach 0 in a while-loop and with thread yield, but will stop after a given duration
        /// \param duration duration to wait for, in seconds
        /// \return true iif the counter reached 0
        bool busyWaitFor(const std::chrono::duration<float>& duration);

        /// Waits for the counter, but puts the thread to sleep
        void sleepWait();

        /// Yields the given fiber until the counter reaches 0.
        /// Does nothing if the counter is already at 0.
        /// The given task WILL be awakened as soon as the counter reaches 0, which ever thread that happens on.
        void wait(Cider::FiberHandle& fiber);

    public:
        /// Increments this counter
        void increment();

        /// Increments this counter
        void increment(std::uint32_t amount);

        /// Decrements this counter
        void decrement();

        /// Is this counter at 0 ?
        bool isIdle() const;

        Counter& operator=(const Counter& other) = delete;

    public: // support for coroutines
        void onCoroutineSuspend(std::coroutine_handle<> h);

        CounterLanePair resumeOnLane(const TaskLane& lane);

    private:
        std::atomic_uint32_t internalCounter;
        std::coroutine_handle<> toContinue = nullptr;

        std::mutex sleepLock;
        std::condition_variable_any sleepCondition;

        mutable ReadWriteLock counterLock;
        std::source_location source; // helps debug
        Cider::SpinLock waitQueueLock;
        Cider::WaitQueue fibersWaiting;

        template<typename T>
        friend class CoroutinePromiseType;
    };
}
