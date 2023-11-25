//
// Created by jglrxavpok on 30/01/2022.
//

#pragma once

#include <atomic>
#include <compare>
#include <functional>

namespace Carrot::Async {
    /// Represents different group on which a task can run
    class TaskLane {
    public:
        TaskLane();
        TaskLane(const TaskLane& lane) = default;

        auto operator<=>(const TaskLane& other) const {
            return id <=> other.id;
        }

        bool operator==(const TaskLane& other) const {
            return id == other.id;
        }

    public:
        /// Task is run during on the current thread
        static TaskLane Undefined;

    private:
        std::uint64_t id = 0;
        static std::atomic_uint64_t counter;

        friend class ::std::hash<Carrot::Async::TaskLane>;
    };

    /**
     * Executes 'count' tasks in parallel, executing 'forEach' for each task.
     * The calling thread will also participate.
     * Waits until all tasks are done before returning
     * @param count how many tasks to execute
     * @param forEach what to execute for each task
     * @param granularity how many tasks at once per thread
     */
    inline void (*parallelFor)(std::size_t count, const std::function<void(std::size_t)>& forEach, std::size_t granularity) = nullptr;
}