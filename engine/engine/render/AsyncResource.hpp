//
// Created by jglrxavpok on 26/01/2022.
//

#pragma once

#include <memory>
#include <thread>
#include <core/Macros.h>
#include <engine/Engine.h>
#include <core/async/Coroutines.hpp>

namespace Carrot {
    template<typename T, bool WaitOnAccess>
    class AsyncResource {
    public:
        using TaskType = Carrot::Async::Task<std::shared_ptr<T>>;

    public:
        AsyncResource() = default;

        AsyncResource(TaskType&& loadingTask): initializer(loadingTask) {
            TODO // GetTaskScheduler().schedule({})
        }

        bool isReady() const {
            return initialized && !!resource;
        }

    public: // access
        const T& operator->() const {
            throwOrWait();
            return *resource;
        }

        T& operator->() {
            throwOrWait();
            return *resource;
        }

        const T& operator*() const {
            throwOrWait();
            return *resource;
        }

        T& operator*() {
            throwOrWait();
            return *resource;
        }

    private:
        void throwOrWait() {
            if constexpr(WaitOnAccess) {
                while(!isReady()) {
                    std::current_thread::yield();
                }
            } else {
                verify(isReady(), "Not ready");
            }
        }

    private:
        std::shared_ptr<T> resource = nullptr;
        TaskType initializer;
    };

    using AsyncModelResource = AsyncResource<Carrot::Model, false>;
}