//
// Created by jglrxavpok on 11/09/2021.
//

#pragma once

#include <coroutine>
#include <vector>
#include <concepts>
#include <mutex>
#include <memory>
#include <engine/utils/Concepts.hpp>
#include <engine/utils/Macros.h>

#ifdef __CLION_IDE_
#include <experimental/coroutine>
#endif

namespace Carrot::Coroutines {
    template<typename Awaiter>
    concept IsAwaiter = requires(Awaiter a, std::coroutine_handle<> h) {
        { a.await_ready() } -> std::convertible_to<bool>;
        { a.await_suspend(h) };
        { a.await_resume() } -> std::convertible_to<void>;
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
                completed.push_back(*it);
            }
            handles.erase(removedStart, handles.end());
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

        ~DeferringAwaiter() {
            cleanup();
        }

    private:
        std::vector<std::coroutine_handle<>> handlesToAdd;
        std::vector<std::coroutine_handle<>> handles;
        std::vector<std::coroutine_handle<>> completed;
        std::mutex handlesAccess;
        std::mutex handlesToAddAccess;
    };

    template<typename Value = void>
    class Task;

    template<typename Value>
    struct PromiseTypeReturn;

    template<>
    struct PromiseTypeReturn<void> {
        void return_void() {

        }
    };

    template<typename Value>
    struct PromiseTypeReturn {
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

        Task<Value> get_return_object() {
            return Task<Value>(std::coroutine_handle<CoroutinePromiseType<Value>>::from_promise(*this));
        }
        std::suspend_never initial_suspend() { return {}; }

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
            return transfer_awaitable{awaiting};
        }

        template<typename Awaiter>
        Awaiter& await_transform(Awaiter& awaiter) requires IsAwaiter<Awaiter> {
            return awaiter;
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
            // TODO std::current_exception();
        }

    };

    template<typename Value>
    class Task {
    public:
        using promise_type = CoroutinePromiseType<Value>;

        std::coroutine_handle<promise_type> coroutineHandle;

        ~Task() {
            /*if(coroutineHandle) {
                coroutineHandle.destroy();
            }*/
        }

        [[nodiscard]] bool done() const {
            return coroutineHandle.done();
        }

        void operator()() {
            coroutineHandle();
        }

/*        operator std::coroutine_handle<promise_type>() {
            return coroutineHandle;
        }

        operator const std::coroutine_handle<promise_type>() const {
            return coroutineHandle;
        }*/

        Value getValue() const requires Carrot::Concepts::NotSame<void, Value> {
            return coroutineHandle.promise().value;
        }

    private:
        explicit Task(std::coroutine_handle<promise_type> handle): coroutineHandle(handle) {}

        friend promise_type;
    };

    /*
     Task<SomeType> myTask(Arg0 arg0, Arg1 arg1) {
        // do stuff
        co_await engine.nextFrameAwaiter();
        // or
        engine.cowaitNextFrame();
        // do your stuff which will execute at the beginning of the next frame.
        co_return SomeTypeValue;
     }

     int main() {
        auto task = myTask();
        SomeJobList.push_back(task);
        // do other stuff
        return 0;
     }

     void handleJobs() {
        for(auto& job : SomeJobList) {
            job();
        }
        SomeJobList.erase(std::remove_if(..., [](auto& job) { return job.done(); }));
     }

     Task must be invocable (will invoke underlying coroutine_handle) -> implicitly convertible to coroutine_handle
    */
}