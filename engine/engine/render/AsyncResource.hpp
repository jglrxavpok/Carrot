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
        AsyncResource() {
            storage = std::make_shared<Storage>();
        };

        AsyncResource(const AsyncResource& toCopy) = delete;
        AsyncResource(AsyncResource&& toMove) = default;

        explicit AsyncResource(TaskType&& loadingTask) {
            storage = std::make_shared<Storage>();
            storage->initializer = std::move(loadingTask);
            TaskDescription desc {
                .name = "Loading AsyncResource",
                .task = asTask(storage),
                // TODO: joiner counter?
            };
            GetTaskScheduler().schedule(std::move(desc));
            //asTask(storage->initializer, storage.get()).resume();
        }

        bool isReady() const {
            return storage->initialized && !!storage->value;
        }

        void forceWait() const {
            verify(storage->initializer || storage->running || storage->initialized, "No initializer task!");
            while(!isReady()) {
                std::this_thread::yield();
            }
        }

    public: // access
        const T* operator->() const {
            throwOrWait();
            return storage->value.get();
        }

        T* operator->() {
            throwOrWait();
            return storage->value.get();
        }

        const T& operator*() const {
            throwOrWait();
            return *storage->value;
        }

        T& operator*() {
            throwOrWait();
            return *storage->value;
        }

    public:
        AsyncResource& operator=(const AsyncResource&) = delete;
        AsyncResource& operator=(AsyncResource&& toMove) {
            storage = toMove.storage;

            toMove.storage = nullptr;
            return *this;
        }

    private:
        struct Storage {
            std::shared_ptr<T> value;
            std::atomic<bool> initialized = false;
            std::atomic<bool> running = false;
            TaskType initializer;
        };

        void throwOrWait() const {
            if constexpr(WaitOnAccess) {
                while(!isReady()) {
                    std::this_thread::yield();
                }
            } else {
                verify(isReady(), "Not ready");
            }
        }

        /// Creates a coroutine compatible with the TaskScheduler
        Async::Task<> asTask(std::shared_ptr<AsyncResource::Storage> targetStorage) {
            verify(targetStorage->initializer, "Needs an initializer");
            verify(!targetStorage->initialized, "Resource must not already be initialized");
            verify(!targetStorage->value, "Resource must not already be initialized");
            targetStorage->running = true;
            targetStorage->value = co_await targetStorage->initializer;
            targetStorage->initializer = {};
            targetStorage->running = false;
            targetStorage->initialized = true;
            co_return;
        }

    private:
        std::shared_ptr<Storage> storage;
    };

    using AsyncModelResource = AsyncResource<Carrot::Model, false>;
}