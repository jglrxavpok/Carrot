//
// Created by jglrxavpok on 11/09/2021.
//

#pragma once

#include <coroutine>
#include <functional>
#include <vector>
#include <list>
#include <concepts>
#include <mutex>
#include <memory>
#include <core/utils/Concepts.hpp>
#include <core/Macros.h>
#include <core/async/Counter.h>
#include <iostream>
#include <core/tasks/Tasks.h>

#ifdef __CLION_IDE_
#include <experimental/coroutine>
#endif

// TODO: Add a way to debug coroutines that are suspended while holding a lock

namespace Carrot::Async {
    template<typename Awaiter>
    concept IsAwaiter = requires(Awaiter a, std::coroutine_handle<> h) {
        { a.await_ready() } -> std::convertible_to<bool>;
        { a.await_suspend(h) };
        { a.await_resume() } -> std::convertible_to<void>;
    };

    static_assert(IsAwaiter<std::suspend_always>, "suspend_always must be considered an awaiter.");

    template<typename T>
    struct CoroutinePromiseType;
    
    template<typename Value = void>
    class Task;

    template<typename Owner, typename ResultType>
    concept TaskOwner = requires(Owner owner, Task<ResultType> task) {
        { owner.transferOwnership(std::move(task)) };
    };

    template<typename Value>
    struct CoroutinePromiseType;

    class AlwaysSuspend {
    public:
        constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {}

        constexpr void await_resume() const noexcept {}
    };

    class DeferringAwaiter {
    public:
        constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            std::lock_guard l {handlesToAddAccess};
            handlesToAdd.push_back(h);
        }

        constexpr void await_resume() const noexcept {}

        void resume_all() {
            std::lock_guard l {handlesAccess};
            {
                std::lock_guard l1 {handlesToAddAccess};
                for(const auto& h : handlesToAdd) {
                    handles.push_back(h);
                }
                handlesToAdd.clear();
            }
            for(auto& handle : handles) {
                handle();
            }
            auto removedStart = std::remove_if(WHOLE_CONTAINER(handles), [](const auto& h) { return h.done(); });
            for(auto it = removedStart; it != handles.end(); it++) {
                auto& h = *it;
                completed.push_back(*it);
            }
            handles.erase(removedStart, handles.end());

            updateOwnedCompletedTasks();
        }

        /// Forces cleanup of all handles inside this awaiter
        void cleanup() {
            std::lock_guard l {handlesAccess};
            for(auto& h : handles) {
                if(h && h.done()) {
                    h.destroy();
                }
            }
            handles.clear();
            for(auto& h : completed) {
                if(h && h.done()) {
                    h.destroy();
                }
            }
            completed.clear();
        }

        void transferOwnership(Async::Task<>&& task);

        ~DeferringAwaiter() {
            cleanup();
        }

    private:
        // destroys tasks owned by this object that have finished
        void updateOwnedCompletedTasks();

        std::vector<std::coroutine_handle<>> handlesToAdd;
        std::vector<std::coroutine_handle<>> handles;
        std::vector<std::coroutine_handle<>> completed;
        std::list<Task<>> ownedTasks;
        std::mutex handlesAccess;
        std::mutex handlesToAddAccess;
    };

    struct BasePromise {
        const Counter* waitOnCounter = nullptr;
    };

    template<typename Value>
    struct PromiseTypeReturn;

    template<>
    struct PromiseTypeReturn<void>: public BasePromise {
        void return_void() {

        }
    };

    template<typename Value>
    struct PromiseTypeReturn: public BasePromise {
        Value value;

        template<std::convertible_to<Value> From>
        void return_value(From&& v) requires Carrot::Concepts::NotSame<void, Value> {
            value = v;
        }

        template<std::convertible_to<Value> From>
        std::suspend_always yield_value(From&& v) requires Carrot::Concepts::NotSame<void, Value> {
            value = v;
            return {};
        }

        Value getValue() const requires Carrot::Concepts::NotSame<void, Value> {
            return value;
        }
    };

    // https://stackoverflow.com/a/67944896
    template<typename Value>
    struct CoroutinePromiseType: public PromiseTypeReturn<Value> {
        std::coroutine_handle<> awaiting;
        TaskLane wantedLane = TaskLane::Undefined;

        Task<Value> get_return_object() {
            return Task<Value>(std::coroutine_handle<CoroutinePromiseType<Value>>::from_promise(*this));
        }

        std::suspend_always initial_suspend() { return {}; }

        // when final suspend is executed 'value' is already set
        // we need to suspend this coroutine in order to use value in other coroutine or through 'get' function
        // otherwise promise object would be destroyed (together with stored value) and one couldn't access task result
        // value
        auto final_suspend() noexcept
        {
            // if there is a coroutine that is awaiting on this coroutine resume it
            struct transfer_awaitable
            {
                std::coroutine_handle<> awaiting_coroutine;

                // always stop at final suspend
                bool await_ready() noexcept
                {
                    return false;
                }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept
                {
                    // resume awaiting coroutine or if there is no coroutine to resume return special coroutine that do
                    // nothing
                    return awaiting_coroutine ? awaiting_coroutine : std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return transfer_awaitable{this->awaiting};
        }

        template<typename Awaiter>
        Awaiter&& await_transform(Awaiter&& awaiter) requires IsAwaiter<Awaiter> {
            return std::forward<Awaiter&&>(awaiter);
        }

        auto await_transform(Counter& counter)
        {
            struct task_awaitable
            {
                CoroutinePromiseType<Value>& promise;
                Counter& counter;

                // check if this task already has value computed
                bool await_ready()
                {
                    return counter.isIdle();
                }

                // h - is a handle to coroutine that calls co_await
                // store coroutine handle to be resumed after computing task value
                void await_suspend(std::coroutine_handle<> h)
                {
                    counter.onCoroutineSuspend(h);
                    promise.waitOnCounter = &counter;
                }

                // when ready return value to a consumer
                auto await_resume()
                {
                    return;
                }
            };

            return task_awaitable{*this, counter};
        }

        /// Wait for a counter and ask to be rescheduled on a different lane
        auto await_transform(CounterLanePair& counterLanePair) {
            struct task_awaitable {
                CoroutinePromiseType<Value>& promise;
                TaskLane lane;
                Counter& counter;

                // check if this task already has value computed
                bool await_ready() {
                    return false;
                }

                // h - is a handle to coroutine that calls co_await
                // store coroutine handle to be resumed after computing task value
                void await_suspend(std::coroutine_handle<> h) {
                    counter.onCoroutineSuspend(h);
                    promise.waitOnCounter = &counter;
                    promise.lane = lane;
                }

                // when ready return value to a consumer
                auto await_resume() {
                    return;
                }
            };

            return task_awaitable{*this, counterLanePair.lane, counterLanePair.counter};
        }

        /// Ask to be rescheduled on a different lane
        auto await_transform(TaskLane gotoLane) {
            struct task_awaitable {
                CoroutinePromiseType<Value>& promise;
                TaskLane lane;

                // check if this task already has value computed
                bool await_ready() {
                    return false;
                }

                // h - is a handle to coroutine that calls co_await
                // store coroutine handle to be resumed after computing task value
                void await_suspend(std::coroutine_handle<> h) {
                    promise.lane = lane;
                }

                // when ready return value to a consumer
                auto await_resume() {
                    return;
                }
            };

            return task_awaitable{*this, gotoLane};
        }

        // also we can await other task<T>
        template<typename U>
        auto await_transform(const Task<U>& task)
        {
            if (!task.coroutineHandle) {
                throw std::runtime_error("coroutine without promise awaited");
            }
            if (task.coroutineHandle.promise().awaiting) {
                throw std::runtime_error("coroutine already awaited");
            }

            struct task_awaitable
            {
                std::coroutine_handle<CoroutinePromiseType<U>> handle;

                // check if this task already has value computed
                bool await_ready()
                {
                  //  return handle.promise().value.has_value();
                    return false;
                }

                // h - is a handle to coroutine that calls co_await
                // store coroutine handle to be resumed after computing task value
                void await_suspend(std::coroutine_handle<> h)
                {
                    handle.promise().awaiting = h;
                    handle.resume();
                }

                // when ready return value to a consumer
                auto await_resume()
                {
                    if constexpr(std::is_same_v<void, U>) {
                        return;
                    } else {
                        return std::move(handle.promise().value);
                    }
                }
            };

            return task_awaitable{task.coroutineHandle};
        }

        void unhandled_exception() {
            throw;
        }

        // called when coroutine_handle::destroy is called
        ~CoroutinePromiseType() {

        }

    };

    /// Coroutine-based async Task
    /// Always suspend on init. The execution will not start before the coroutine is resumed (via resume or operator())
    /// Coroutine handle is destroyed when reaching the destructor (RAII).
    /// Move operator and constructor both allow to transfer ownership (can be used to move coroutine to another thread without keeping the original Task)
    template<typename Value>
    class Task {
    public:
        using promise_type = CoroutinePromiseType<Value>;

        std::coroutine_handle<promise_type> coroutineHandle;

        Task() = default;

        // copy not allowed
        Task(const Task<Value>&) = delete;
        Task& operator=(const Task<Value>&) = delete;

        Task(Task<Value>&& toMove) {
            *this = std::move(toMove);
        }

        Task& operator=(Task<Value>&& toMove)  noexcept {
            if(coroutineHandle) {
                coroutineHandle.destroy();
            }
            coroutineHandle = toMove.coroutineHandle;
            toMove.coroutineHandle = nullptr;
            return *this;
        }

        ~Task() {
            if(coroutineHandle) {
                coroutineHandle.destroy();
                coroutineHandle = nullptr;
            }
        }

        [[nodiscard]] bool done() const {
            verify(coroutineHandle, "Must have an handle");
            return coroutineHandle.done();
        }

        explicit operator bool() const {
            return !!coroutineHandle;
        }

        TaskLane wantedLane() const {
            return coroutineHandle.promise().wantedLane;
        }

        /// Resumes the coroutine if possible. If the coroutine is waiting for a counter, this will return 'false', and not resume the coroutine
        bool operator()(TaskLane currentLane = TaskLane::Undefined) {
            return resume(currentLane);
        }

        /// Resumes the coroutine if possible. If the coroutine is waiting for a counter, this will return 'false', and not resume the coroutine
        bool resume(TaskLane currentLane = TaskLane::Undefined) {
            verify(coroutineHandle, "Must have an handle");
            if(coroutineHandle.promise().waitOnCounter && !coroutineHandle.promise().waitOnCounter->isIdle()) {
                return false;
            }
            if(coroutineHandle.promise().wantedLane != TaskLane::Undefined && currentLane != coroutineHandle.promise().wantedLane) {
                return false;
            }
            coroutineHandle();
            return true;
        }

        Value getValue() const requires Carrot::Concepts::NotSame<void, Value> {
            verify(coroutineHandle, "Must have an handle");
            return coroutineHandle.promise().value;
        }

        template<typename Owner>
        void transferTo(Owner& owner) requires TaskOwner<Owner, Value> {
            owner.transferOwnership(std::move(*this));
        }

    private:
        explicit Task(std::coroutine_handle<promise_type> h): coroutineHandle(h) {}

        friend promise_type;
    };

    template<typename ReturnType>
    inline Task<ReturnType> AsTask(std::function<ReturnType()> function) {
        if constexpr(std::is_same_v<void, ReturnType>) {
            function();
            co_return;
        } else {
            co_return function();
        }
    };

}