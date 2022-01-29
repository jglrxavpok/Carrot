//
// Created by jglrxavpok on 26/01/2022.
//

#pragma once

#include <atomic>
#include <coroutine>
#include <chrono>
#include <mutex>
#include <core/Macros.h>

namespace Carrot::Async {
    template<class T>
    struct CoroutinePromiseType;

    /// Counter to determine when tasks are done
    class Counter {
    public:
        Counter() = default;
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

    public:
        /// Increments this counter
        void increment();

        /// Decrements this counter
        void decrement();

        /// Is this counter at 0 ?
        bool isIdle() const;

    public: // support for coroutines
        void onCoroutineSuspend(std::coroutine_handle<> h);

    private:
        std::atomic_uint32_t internalCounter;
        std::coroutine_handle<> toContinue;

        std::mutex sleepLock;
        std::condition_variable sleepCondition;

        template<typename T>
        friend class CoroutinePromiseType;
    };
}
