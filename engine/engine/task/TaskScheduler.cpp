//
// Created by jglrxavpok on 27/01/2022.
//

#include "TaskScheduler.h"
#include "engine/utils/Profiling.h"
#include <core/async/OSThreads.h>
#include "engine/utils/Macros.h"
#include "engine/Engine.h"
#include "engine/render/VulkanRenderer.h"

namespace Carrot {
    Async::TaskLane TaskScheduler::Parallel;
    Async::TaskLane TaskScheduler::MainLoop;
    Async::TaskLane TaskScheduler::Rendering;

    TaskScheduler::TaskScheduler() {
        std::size_t availableThreads = std::thread::hardware_concurrency() - 1 /* main thread */;
        parallelThreads.resize(availableThreads);
        for (std::size_t i = 0; i < availableThreads; i++) {
            parallelThreads[i] = std::thread([this]() {
                GetRenderer().makeCurrentThreadRenderCapable();
                threadProc();
            });
            Carrot::Threads::setName(parallelThreads[i], Carrot::sprintf("ParallelTask #%d", i+1));
        }

        mainLoopScheduling = coscheduleSingleTask(taskQueues[TaskScheduler::MainLoop], TaskScheduler::MainLoop);
        renderingScheduling = coscheduleSingleTask(taskQueues[TaskScheduler::Rendering], TaskScheduler::Rendering);
    }

    TaskScheduler::~TaskScheduler() {
        running = false;
        for (auto& t : parallelThreads) {
            t.join();
        }
    }

    [[nodiscard]] Async::Task<> TaskScheduler::coscheduleSingleTask(ThreadSafeQueue<TaskDescription>& taskQueue, Async::TaskLane localLane) {
        TaskDescription toRun;
        while(true) {
            if(taskQueue.popSafe(toRun)) {
                bool canRun = true;

                const Async::TaskLane wantedLane = toRun.task.wantedLane();
                if (wantedLane != Async::TaskLane::Undefined && wantedLane != localLane) {
                    canRun = false;
                    schedule(std::move(toRun), wantedLane);
                }

                if (canRun && toRun.dependency) {
                    if (!toRun.dependency->isIdle()) {
                        taskQueue.push(std::move(toRun));
                        canRun = false;
                    }
                }

                if (canRun) {
                    {
                        ZoneScopedN("Run task");
                        ZoneText(toRun.name.c_str(), toRun.name.size());
                        toRun.task();
                    }

                    if (toRun.task.done()) {
                        if (toRun.joiner) {
                            toRun.joiner->decrement();
                        }
                    } else {
                        taskQueue.push(std::move(toRun));
                    }
                }
            }
            co_await std::suspend_always{};
        }
        co_return;
    }

    void TaskScheduler::threadProc() {
        auto scheduleTask = coscheduleSingleTask(taskQueues[TaskScheduler::Parallel], TaskScheduler::Parallel);
        while(running) {
            scheduleTask();
            std::this_thread::yield();
        }
    }

    void TaskScheduler::scheduleMainLoop() {
        mainLoopScheduling();
    }

    void TaskScheduler::scheduleRendering() {
        renderingScheduling();
    }

    void TaskScheduler::schedule(TaskDescription&& description, Async::TaskLane lane) {
        verify(lane == TaskScheduler::Parallel
               || lane == TaskScheduler::MainLoop
               || lane == TaskScheduler::Rendering
               , "No other lanes supported at the moment");
        verify(description.task, "No valid task");
        verify(!description.task.done(), "Task is already done");
        if(description.joiner) {
            description.joiner->increment();
        }
        {
            ZoneScopedN("Push task description to queue");
            taskQueues[lane].push(std::move(description));
        }
    }
}