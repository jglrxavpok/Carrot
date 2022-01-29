//
// Created by jglrxavpok on 27/01/2022.
//

#pragma once

#include <mutex>
#include <queue>
#include <core/Macros.h>

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

    private:
        std::mutex lock;
        std::queue<T> storage;
    };
}