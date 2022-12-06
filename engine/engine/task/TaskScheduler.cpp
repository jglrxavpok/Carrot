//
// Created by jglrxavpok on 27/01/2022.
//

#include "TaskScheduler.h"
#include "engine/utils/Profiling.h"
#include <core/async/OSThreads.h>
#include "engine/utils/Macros.h"
#include "engine/Engine.h"
#include "engine/render/VulkanRenderer.h"
#include "core/io/Logging.hpp"

namespace Carrot {
    Async::TaskLane TaskScheduler::FrameParallelWork;
    Async::TaskLane TaskScheduler::AssetLoading;
    Async::TaskLane TaskScheduler::MainLoop;
    Async::TaskLane TaskScheduler::Rendering;

    std::size_t TaskScheduler::frameParallelWorkParallelismAmount() {
        // reduce CPU load by using less threads
        return std::thread::hardware_concurrency()/2 - 1 /* main thread */;
    }

    std::size_t TaskScheduler::assetLoadingParallelismAmount() {
        // reduce CPU load by using less threads
        return std::thread::hardware_concurrency()/2 - 1 /* main thread */;
    }

    TaskScheduler::TaskScheduler() {
        const std::size_t inFrameCount = frameParallelWorkParallelismAmount();
        std::size_t availableThreads = inFrameCount + assetLoadingParallelismAmount();
        parallelThreads.resize(availableThreads);
        for (std::size_t i = 0; i < availableThreads; i++) {
            bool isInFrame = i < inFrameCount;
            parallelThreads[i] = std::thread([isInFrame, this]() {
                GetRenderer().makeCurrentThreadRenderCapable();
                threadProc(isInFrame ? FrameParallelWork : AssetLoading);
            });
            Carrot::Threads::setName(parallelThreads[i], Carrot::sprintf("%sParallelTask #%d", isInFrame ? "Frame" : "AssetLoading", i+1));
        }

        mainLoopScheduling = coscheduleSingleTask(taskQueues[TaskScheduler::MainLoop], TaskScheduler::MainLoop, false);
        renderingScheduling = coscheduleSingleTask(taskQueues[TaskScheduler::Rendering], TaskScheduler::Rendering, false);
    }

    TaskScheduler::~TaskScheduler() {
        running = false;
        for(auto& [_, queue] : taskQueues) {
            queue.requestStop();
        }
        for (auto& t : parallelThreads) {
            t.join();
        }
    }

    [[nodiscard]] Async::Task<bool> TaskScheduler::coscheduleSingleTask(ThreadSafeQueue<TaskDescription>& taskQueue, Async::TaskLane localLane, bool allowBlocking) {
        TaskDescription toRun;
        while(true) {
            bool foundSomethingToExecute = allowBlocking
                    ? taskQueue.blockingPopSafe(toRun)
                    : taskQueue.popSafe(toRun);
            if(foundSomethingToExecute) {
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
                        try {
                            toRun.task();
                        } catch (const std::exception& e) {
                            // don't crash thread if a task fails
                            Carrot::Log::error("Error while executing scheduled task '%s': %s", toRun.name.c_str(), e.what());
                        }
                    }

                    if (toRun.task.done()) {
                        if (toRun.joiner) {
                            toRun.joiner->decrement();
                        }
                    } else {
                        taskQueue.push(std::move(toRun));
                    }
                }

                co_yield true;
            }
            co_yield false;
        }
        co_return false;
    }

    void TaskScheduler::threadProc(const Async::TaskLane& lane) {
        auto scheduleTask = coscheduleSingleTask(taskQueues[lane], lane, true);
        while(running) {
            scheduleTask();
            std::this_thread::yield();
        }
    }

    void TaskScheduler::executeMainLoop() {
        mainLoopScheduling();
    }

    void TaskScheduler::executeRendering() {
        renderingScheduling();
    }

    void TaskScheduler::schedule(TaskDescription&& description, const Async::TaskLane& lane) {
        verify(lane == TaskScheduler::FrameParallelWork
               || lane == TaskScheduler::AssetLoading
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