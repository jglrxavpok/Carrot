//
// Created by jglrxavpok on 26/01/2022.
//

#include "Counter.h"

#include <cider/Mutex.h>

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
        increment(1);
    }

    void Counter::increment(std::uint32_t amount) {
        LockGuard l { counterLock.write() };
        verify(amount > 0, "Cannot be < 1");
        internalCounter += amount;
    }

    void Counter::decrement() {
        LockGuard l { counterLock.write() };
        std::uint32_t currentValue = internalCounter--;
        verify(currentValue > 0, "Trying to decrement counter at 0");

        if(currentValue == 1) {
            sleepCondition.notify_all();
            if(toContinue) {
                toContinue();
            }

            Cider::UniqueSpinLock g { waitQueueLock };
            fibersWaiting.notifyAll(g);
        }
    }

    bool Counter::isIdle() const {
        LockGuard l { counterLock.read() };
        return internalCounter == 0;
    }

    void Counter::busyWait() {
        std::uint32_t value = -1;
        do {
            std::this_thread::yield();
            LockGuard l { counterLock.read() };
            value = internalCounter;
        } while(value != 0);
    }

    bool Counter::busyWaitFor(const std::chrono::duration<float>& duration) {
        auto start = std::chrono::steady_clock::now();

        std::uint32_t value = -1;
        do {
            std::this_thread::yield();
            LockGuard l { counterLock.read() };
            value = internalCounter;
            auto now = std::chrono::steady_clock::now();
            if(now - start >= duration) {
                return false;
            }
        } while(value != 0);
        return true;
    }

    void Counter::sleepWait() {
        if(isIdle()) {
            return;
        }
        std::unique_lock<Async::BasicLock> lk{ counterLock.read() };
        sleepCondition.wait(lk, [&] {
            return internalCounter == 0;
        });
    }

    void Counter::wait(Cider::FiberHandle& task) {
        Cider::UniqueSpinLock g { waitQueueLock };
        if(isIdle()) {
            return;
        }
        fibersWaiting.suspendAndWait(g, task);
    }

    void Counter::onCoroutineSuspend(std::coroutine_handle<> h) {
        verify(h, "Coroutine handle must be valid coroutine.");
        verify(!toContinue, "There is already a coroutine waiting for this counter.");
        toContinue = h;
    }

    CounterLanePair Counter::resumeOnLane(const TaskLane& lane) {
        return {
            .counter = *this,
            .lane = lane,
        };
    }

}