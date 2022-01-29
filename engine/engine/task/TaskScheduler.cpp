//
// Created by jglrxavpok on 27/01/2022.
//

#include "TaskScheduler.h"
#include "engine/utils/Profiling.h"

namespace Carrot {
    TaskScheduler::TaskScheduler() {
        std::size_t availableThreads = std::thread::hardware_concurrency() - 1 /* main thread */;
        parallelThreads.resize(availableThreads);
        for (std::size_t i = 0; i < availableThreads; i++) {
            parallelThreads[i] = std::thread([this]() {
               threadProc();
            });
        }
    }

    TaskScheduler::~TaskScheduler() {
        running = false;
        for (auto& t : parallelThreads) {
            t.join();
        }
    }

    void TaskScheduler::threadProc() {
        TaskDescription toRun;
        while(running) {
            if(!tasks.popSafe(toRun)) {
                std::this_thread::yield();
                continue;
            }

            bool canRun = true;
            if(toRun.dependency) {
                if(!toRun.dependency->isIdle()) {
                    tasks.push(std::move(toRun));
                    canRun = false;
                }
            }

            if(canRun) {
                {
                    ZoneScopedN("Run task");
                    ZoneText(toRun.name.c_str(), toRun.name.size());
                    toRun.task();
                }

                if(toRun.task.done()) {
                    if(toRun.joiner) {
                        toRun.joiner->decrement();
                    }
                } else {
                    tasks.push(std::move(toRun));
                }
            }

            std::this_thread::yield();
        }
    }

    void TaskScheduler::schedule(TaskDescription&& description, TaskLane lane) {
        verify(lane == TaskLane::Parallel, "No other lanes supported at the moment");
        verify(description.task, "No valid task");
        verify(!description.task.done(), "Task is already done");
        if(description.joiner) {
            description.joiner->increment();
        }
        tasks.push(std::move(description));
    }
}