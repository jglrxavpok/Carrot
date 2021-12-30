//
// Created by jglrxavpok on 21/03/2021.
//

#pragma once
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>

namespace Carrot {
    template<typename T>
    class ThreadLocal {
    private:
        std::mutex valueGuard;
        std::unordered_map<std::thread::id, T> value;
        std::function<T()> initializer = []() { return T{}; };

    public:
        explicit ThreadLocal() = default;
        explicit ThreadLocal(std::function<T()> initializer): initializer(initializer) {};

        operator T&() {
            return get();
        }

        T& get() {
            auto id = std::this_thread::get_id();
            auto iterator = value.find(id);
            if(iterator == value.end()) {
                std::lock_guard lk(valueGuard);
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