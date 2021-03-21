//
// Created by jglrxavpok on 21/03/2021.
//

#pragma once
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>

namespace Carrot {
    using namespace std;

    template<typename T>
    class ThreadLocal {
    private:
        mutex valueGuard;
        unordered_map<thread::id, T> value;
        function<T()> initializer = []() { return T{}; };

    public:
        explicit ThreadLocal() = default;
        explicit ThreadLocal(function<T()> initializer): initializer(initializer) {};

        operator T&() {
            return get();
        }

        T& get() {
            auto id = this_thread::get_id();
            auto iterator = value.find(id);
            if(iterator == value.end()) {
                lock_guard lk(valueGuard);
                {
                    auto it = value.find(id);
                    if(it == value.end()) {
                        value[id] = initializer();
                    }
                }
            }
            return value.at(id);
        }
    };
}