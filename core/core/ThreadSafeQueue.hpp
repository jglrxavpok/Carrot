//
// Created by jglrxavpok on 27/01/2022.
//

#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <core/Macros.h>
#include "async/Locks.h"

namespace Carrot {

    /// Queue that ensures access from multiple threads at once will not break the structure
    template<typename T>
    class ThreadSafeQueue {
    public:
        ThreadSafeQueue() {};

    public:
        bool isEmpty() {
            std::lock_guard lg { lock };
            return storage.empty();
        }

        std::size_t getSize() {
            std::lock_guard lg { lock };
            return storage.size();
        }

        /// Adds the given object at the end of the queue
        void push(T&& object) {
            std::lock_guard lg { lock };
            storage.push(std::forward<T&&>(object));
            conditionVariable.notify_all();
        }

        /// Returns a copy of the object at the front of the queue. Throws is the queue is empty
        T pop() {
            T out;
            pop(out);
            return out;
        }

        /// Copies the object at the front of the queue inside 'out'. Throws is the queue is empty
        void pop(T& out) {
            std::lock_guard lg { lock };
            verify(!storage.empty(), "Queue is empty!");
            out = std::move(storage.front());
            storage.pop();
        }

        /// Copies the object at the front of the queue inside 'out'.
        /// Returns false and does not modify 'out' if the queue is empty
        bool popSafe(T& out) {
            std::lock_guard lg { lock };
            if(storage.empty()) {
                return false;
            }
            out = std::move(storage.front());
            storage.pop();
            return true;
        }

        bool blockingPopSafe(T& out) {
            std::unique_lock lk { lock };
            while(storage.empty() && !stopRequested.load()) {
                conditionVariable.wait(lk);
            }

            bool empty = storage.empty();
            if(!empty) {
                out = std::move(storage.front());
                storage.pop();
            }
            lk.unlock();
            return !empty;
        }

        void requestStop() {
            stopRequested.store(true);
            conditionVariable.notify_all();
        }

    private:
        std::mutex lock;
        std::queue<T> storage;

        std::condition_variable conditionVariable;
        std::atomic_bool stopRequested { false };
    };
}