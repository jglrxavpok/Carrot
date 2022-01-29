//
// Created by jglrxavpok on 27/01/2022.
//

#pragma once

#include <vector>
#include <core/ThreadSafeQueue.hpp>
#include <core/async/Counter.h>
#include <core/async/Coroutines.hpp>

namespace Carrot {
    class Engine;

    /// Represents different group on which a task can run
    enum class TaskLane {
        /// Group of threads that run parallel to the main thread
        Parallel,
    };

    struct TaskDescription {
        /// Name/Description of the task.
        std::string name;

        /// Your task to execute.
        Async::Task<> task;

        /// Counter to wait for before starting this task. If null, no wait is performed and the task starts as soon as
        /// it is scheduled
        Async::Counter* dependency = nullptr;

        /// Incremented when the task is scheduled, decremented when the task is finished
        Async::Counter* joiner = nullptr;
    };

    class TaskScheduler {
    public:
        /// Schedule a task for execution. The task will be executed as soon as possible on the given lane.
        void schedule(TaskDescription&& description, TaskLane lane = TaskLane::Parallel);

    private:
        TaskScheduler();
        ~TaskScheduler();

        void threadProc();

    private:
        ThreadSafeQueue<TaskDescription> tasks;
        std::atomic<bool> running = true;
        std::vector<std::thread> parallelThreads;

        friend class Engine;
    };
}
