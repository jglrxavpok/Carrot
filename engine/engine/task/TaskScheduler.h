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
#include <cider/Fiber.h>
#include <cider/GrowingStack.h>
#include <cider/scheduling/Scheduler.h>
#include <concurrentqueue.h>
#include <blockingconcurrentqueue.h>
#include <core/containers/Vector.hpp>

namespace Carrot {
    class Engine;
    struct TaskData;
    class TaskHandle;

    using TaskProc = std::function<void(TaskHandle&)>;

    /**
     * If this is an argument of a function, it most often means that the function is expected to be called from a task,
     * executing on a fiber, scheduled via the TaskScheduler.
     * It also means that the function very probably yields the current fiber.
     */
    class TaskHandle {
    public:
        /**
         * Change task lane where this task is executed.
         * Does nothing
         * @param resumeOn
         */
        void changeLane(const Async::TaskLane& resumeOn);

        /**
         * Yield this task and reschedule it to be re-executed later (allows other tasks to progress)
         */
        void yield();

        /**
         * Wait for the counter to reach 0, by yielding. Does nothing if the counter is already at 0
         * @param counter
         */
        void wait(Async::Counter& counter);

        /**
         * Gets the underlying fiber handle. Use only if you know what you are doing.
         */
        Cider::FiberHandle& getFiberHandle();

        /**
         * Implicit conversion to FiberHandle, intended for usage with locks and other synchronisation primitives from Cider
         */
        operator Cider::FiberHandle&();

        /**
         * Which lane is this task currently running on?
         */
        const Async::TaskLane& getCurrentLane();

    private:
        explicit TaskHandle(Cider::FiberHandle& fiberHandle, TaskData& selfData);

        Cider::FiberHandle& fiberHandle;
        TaskData& taskData;

        friend class TaskScheduler;
    };

    struct TaskDescription {
        /// Name/Description of the task.
        std::string name;

        /// Your task to execute.
        TaskProc task;

        /// Counter to wait for before starting this task. If null, no wait is performed and the task starts as soon as
        /// it is scheduled
        Async::Counter* dependency = nullptr;

        /// Incremented when the task is scheduled, decremented when the task is finished
        Async::Counter* joiner = nullptr;
    };

    struct TaskData : public TaskDescription, public std::enable_shared_from_this<TaskData> {
        //char stack[4096*16] { 0 };
        Cider::GrowingStack stack { 64 * 1024 * 1024 };
        Async::TaskLane currentLane = Async::TaskLane::Undefined;
        Async::TaskLane wantedLane = Async::TaskLane::Undefined;
        std::unique_ptr<Cider::Fiber> fiber = nullptr;

        TaskData();
        ~TaskData();
    };

    class TaskScheduler {
    public:
        /**
         * Executes 'count' tasks in parallel, executing 'forEach' for each task.
         * The calling thread will also participate.
         * Waits until all tasks are done before returning
         * @param count how many tasks to execute
         * @param forEach what to execute for each task
         * @param granularity how many tasks at once per thread
         */
        void parallelFor(std::size_t count, const std::function<void(std::size_t)>& forEach, std::size_t granularity);

        Carrot::Vector<std::thread::id> getParallelThreadIDs() const;

    public: // scheduling
        /// Schedule a task for execution. The task will be executed as soon as possible on the given lane.
        void schedule(TaskDescription&& description, const Async::TaskLane& lane);

        /// Schedule a task for execution. Call from the main loop
        void executeMainLoop();

        /// Schedule a task for execution. Call at the beginning of rendering
        void executeRendering();

        /// Steals a job from the given lane and runs it.
        /// If there are no jobs to steal, does nothing
        void stealJobAndRun(const Async::TaskLane& lane);

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

        std::shared_ptr<TaskData> getOrReuseTaskData();
        void runSingleTask(Async::TaskLane lane, bool allowBlocking);
        void threadProc(const Async::TaskLane& lane);

    private:
        class FiberScheduler: public Cider::Scheduler {
        public:
            explicit FiberScheduler(TaskScheduler& taskScheduler): taskScheduler(taskScheduler) {}

            void schedule(Cider::FiberHandle& toSchedule) override final;

            void schedule(Cider::FiberHandle& toSchedule, Cider::Proc proc, void *userData) override final;

            void schedule(Cider::FiberHandle& toSchedule, const Cider::StdFunctionProc& proc) override final;

        private:
            TaskScheduler& taskScheduler;
        };
        FiberScheduler fiberScheduler { *this };

        using TaskQueue = moodycamel::BlockingConcurrentQueue<std::shared_ptr<TaskData>>;
        std::unordered_map<Async::TaskLane, TaskQueue> taskQueues;

        TaskQueue reusableTaskData;
        std::atomic<bool> running = true;
        std::vector<std::thread> parallelThreads;

        friend class Engine;
        friend class FiberScheduler;
        friend class TaskHandle;
    };
}
