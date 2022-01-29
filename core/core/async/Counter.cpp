//
// Created by jglrxavpok on 26/01/2022.
//

#include "Counter.h"
#include "Coroutines.hpp"

namespace Carrot::Async {
    Counter::~Counter() {
        verify(internalCounter == 0, "Destroying a counter that has not yet reached 0");
    }

    Counter::Counter(Counter&& toMove) {
        std::uint32_t valueToSet = toMove.internalCounter.load();
        while(!toMove.internalCounter.compare_exchange_strong(valueToSet, 0)) {
            valueToSet = toMove.internalCounter.load();
        }
    }

    void Counter::increment() {
        internalCounter++;
    }

    void Counter::decrement() {
        verify(internalCounter > 0, "Cannot decrement a counter that is at 0!");
        internalCounter--;

        if(internalCounter == 0) {
            sleepCondition.notify_one();
            if(toContinue) {
                toContinue();
            }
        }
    }

    bool Counter::isIdle() const {
        return internalCounter == 0;
    }

    void Counter::busyWait() {
        while(internalCounter != 0) {
            std::this_thread::yield();
        }
    }

    bool Counter::busyWaitFor(const std::chrono::duration<float>& duration) {
        auto start = std::chrono::steady_clock::now();
        while(internalCounter != 0) {
            auto now = std::chrono::steady_clock::now();
            if(now - start >= duration) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    void Counter::sleepWait() {
        std::unique_lock lk(sleepLock);
        sleepCondition.wait(lk, [&]{ return internalCounter == 0; });
    }

    void Counter::onCoroutineSuspend(std::coroutine_handle<> h) {
        verify(h, "Coroutine handle must be valid coroutine.");
        verify(!toContinue, "There is already a coroutine waiting for this counter.");
        toContinue = h;
    }

}