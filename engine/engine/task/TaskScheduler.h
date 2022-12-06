//
// Created by jglrxavpok on 27/01/2022.
//

#pragma once

#include <unordered_map>
#include <vector>
#include <core/ThreadSafeQueue.hpp>
#include <core/async/Counter.h>
#include <core/async/Coroutines.hpp>
#include <core/data/Hashes.h>
#include <core/tasks/Tasks.h>

namespace Carrot {
    class Engine;

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
        void schedule(TaskDescription&& description, const Async::TaskLane& lane);

        /// Schedule a task for execution. Call from the main loop
        void executeMainLoop();

        /// Schedule a task for execution. Call at the beginning of rendering
        void executeRendering();

    public:
        /// How many threads can we use for the task scheduler? Only count "short" tasks
        static std::size_t frameParallelWorkParallelismAmount();

        /// How many threads can we use for the task scheduler? Only count "long" tasks
        static std::size_t assetLoadingParallelismAmount();

    public:
        /// Group of threads that run parallel to the main thread. Conceptually must be used for short tasks (that are expected to finish in the current frame)
        static Async::TaskLane FrameParallelWork;

        /// Group of threads that run parallel to the main thread. Conceptually must be used for long tasks (that are NOT expected to finish in the current frame)
        static Async::TaskLane AssetLoading;

        /// Run tasks at each iteration of the main loop
        static Async::TaskLane MainLoop;

        /// Run tasks at the beginning of each frame
        static Async::TaskLane Rendering;

    private:
        TaskScheduler();
        ~TaskScheduler();

        [[nodiscard]] Async::Task<bool> coscheduleSingleTask(ThreadSafeQueue<TaskDescription>& taskQueue, Async::TaskLane lane, bool allowBlocking);
        void threadProc(const Async::TaskLane& lane);

    private:
        std::unordered_map<Async::TaskLane, ThreadSafeQueue<TaskDescription>> taskQueues;
        std::atomic<bool> running = true;
        std::vector<std::thread> parallelThreads;

        Async::Task<bool> mainLoopScheduling;
        Async::Task<bool> renderingScheduling;

        friend class Engine;
    };
}
