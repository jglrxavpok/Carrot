//
// Created by jglrxavpok on 27/01/2022.
//

#include "TaskScheduler.h"
#include "engine/utils/Profiling.h"
#include <core/async/OSThreads.h>
#include "engine/utils/Macros.h"
#include "engine/Engine.h"
#include "engine/render/VulkanRenderer.h"
#include <engine/console/RuntimeOption.hpp>
#include "core/io/Logging.hpp"

static std::atomic<std::int64_t> TaskScheduledThisFrameCount{0};
static std::atomic<std::int64_t> TaskDataCreatedThisFrameCount{0};
static std::atomic<std::int64_t> TaskDataCreatedCount{0};
static std::atomic<std::int64_t> AliveTaskDataCount{0};
static std::atomic<std::int64_t> ActiveTaskCount{0};
static std::atomic<std::int64_t> FiberCreatedCount{0};
static Carrot::RuntimeOption ShowDebug("Debug/Task Scheduler", false);

namespace Carrot {
    Async::TaskLane TaskScheduler::FrameParallelWork;
    Async::TaskLane TaskScheduler::AssetLoading;
    Async::TaskLane TaskScheduler::MainLoop;
    Async::TaskLane TaskScheduler::Rendering;

    struct FiberLocalStorage {
        TaskData* taskData = nullptr;
        std::string* fiberTracyID = nullptr; //< unique ID for Tracy. Allocated once and never deleted, because it may need to survive past 'main' !
        bool isFullyInit = false;
        bool tracyEnteredFiber = false;
    };

    static_assert(sizeof(FiberLocalStorage) <= sizeof(Cider::FiberHandle::localStorage));

    static TaskData& getTaskData(Cider::FiberHandle& fiberHandle) {
        auto* fls = (FiberLocalStorage*) &fiberHandle.localStorage[0];
        return *fls->taskData;
    }

    TaskData::TaskData() {
        TaskDataCreatedCount++;
        TaskDataCreatedThisFrameCount++;
    }

    TaskData::~TaskData() {
        AliveTaskDataCount--;
    }

    void TaskHandle::changeLane(const Async::TaskLane& resumeOn) {
        taskData.wantedLane = resumeOn;
        fiberHandle.yieldOnTop([this]() {
            auto task = this->taskData.shared_from_this();
            GetTaskScheduler().taskQueues[taskData.wantedLane].push(std::move(task));
        });
    }

    void TaskHandle::yield() {
        fiberHandle.yieldOnTop([this]() {
            auto task = this->taskData.shared_from_this();
            GetTaskScheduler().taskQueues[taskData.currentLane].push(std::move(task));
        });
    }

    void TaskHandle::wait(Async::Counter& counter) {
        counter.wait(*this);
    }

    Cider::FiberHandle& TaskHandle::getFiberHandle() {
        return fiberHandle;
    }

    TaskHandle::operator Cider::FiberHandle&() {
        return getFiberHandle();
    }

    const Async::TaskLane& TaskHandle::getCurrentLane() {
        return taskData.currentLane;
    }

    TaskHandle::TaskHandle(Cider::FiberHandle& fiberHandle, TaskData& selfData)
    : fiberHandle(fiberHandle)
    , taskData(selfData) {
    }

    std::size_t TaskScheduler::frameParallelWorkParallelismAmount() {
        // reduce CPU load by using less threads
        return std::thread::hardware_concurrency()/2 - 1 /* main thread */;
    }

    std::size_t TaskScheduler::assetLoadingParallelismAmount() {
        // reduce CPU load by using less threads
        return std::thread::hardware_concurrency()/2 - 1 /* main thread */;
    }

    TaskScheduler::TaskScheduler() {
        Cider::Fiber::OnFiberEnter = [](Cider::Fiber* fiber) {
            auto* fls = (FiberLocalStorage*) &fiber->getHandlePtr()->localStorage[0];
            if(fls && fls->isFullyInit) {
                fls->tracyEnteredFiber = true;
                TracyFiberEnter(fls->fiberTracyID->c_str());
            }
        };
        Cider::Fiber::OnFiberExit = [](Cider::Fiber* fiber) {
            auto* fls = (FiberLocalStorage*) &fiber->getHandlePtr()->localStorage[0];
            if(fls && fls->tracyEnteredFiber) {
                TracyFiberLeave;
            }
        };

        Carrot::Async::parallelFor = [](std::size_t count, const std::function<void(std::size_t)>& forEach, std::size_t granularity) {
            GetTaskScheduler().parallelFor(count, forEach, granularity);
        };

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

    std::shared_ptr<TaskData> TaskScheduler::getOrReuseTaskData() {
        std::shared_ptr<TaskData> taskData;
        if(reusableTaskData.popSafe(taskData)) {
            return taskData;
        }

        taskData = std::make_shared<TaskData>();
        std::string* pTracyID = new std::string{Carrot::sprintf("Fiber %ll", FiberCreatedCount++)};
        auto fiberProc = [pTracyID, pTaskKeepAlive = taskData, pTaskScheduler = this](Cider::FiberHandle& fiber) {
            {
                struct Data {
                    std::shared_ptr<TaskData> pTask;
                    TaskScheduler* pTaskScheduler = nullptr;
                };
                Data data {
                        .pTask = pTaskKeepAlive,
                        .pTaskScheduler = pTaskScheduler
                };

                auto* fls = (FiberLocalStorage*) &fiber.localStorage[0];
                fls->fiberTracyID = pTracyID;
                fls->isFullyInit = true;

                while(true) {
                    // yield this fiber, and sets up task data for reuse
                    fiber.yieldOnTop([](void* pUserData) {
                        Data* pData = (Data*)pUserData;
                        auto pTask = pData->pTask;
                        pData->pTaskScheduler->reusableTaskData.push(std::move(pTask));
                    }, &data);
                }
            }
            verify(false, "Reached bottom of fiber used for tasks, should not happen!");
        };
        taskData->fiber = std::make_unique<Cider::Fiber>(std::move(fiberProc), taskData->stack.asSpan(), fiberScheduler);
        return taskData;
    }

    void TaskScheduler::runSingleTask(Async::TaskLane localLane, bool allowBlocking) {
        std::shared_ptr<TaskData> toRun = nullptr;
        auto& taskQueue = taskQueues[localLane];
        bool foundSomethingToExecute = allowBlocking
                ? taskQueue.blockingPopSafe(toRun)
                : taskQueue.popSafe(toRun);
        if(foundSomethingToExecute) {
            ZoneScopedN("Run task");
            ZoneText(toRun->name.c_str(), toRun->name.size());
            try {
                toRun->currentLane = localLane;
                toRun->fiber->switchTo();

                // handle task lane changes
                if(toRun->wantedLane != localLane) {
                    taskQueues[toRun->wantedLane].push(std::move(toRun));
                }
            } catch (const std::exception& e) {
                // don't crash thread if a task fails
                Carrot::Log::error("Error while executing scheduled task '%s': %s", toRun->name.c_str(), e.what());
            }
        }
    }

    void TaskScheduler::threadProc(const Async::TaskLane& lane) {
        while(running) {
            runSingleTask(lane, true);
        }
    }

    void TaskScheduler::executeMainLoop() {
        runSingleTask(TaskScheduler::MainLoop, false);
    }

    void TaskScheduler::executeRendering() {
        runSingleTask(TaskScheduler::Rendering, false);

        if(ShowDebug) {
            const std::int64_t taskDataCreatedThisFrame = TaskDataCreatedThisFrameCount.exchange(0);
            const std::int64_t tasksScheduledThisFrame = TaskScheduledThisFrameCount.exchange(0);
            if(ImGui::Begin("Task Scheduler")) {
                ImGui::Text("Active tasks: %llu", ActiveTaskCount.load());
                ImGui::Text("Task count scheduled this frame: %llu", tasksScheduledThisFrame);
                ImGui::Text("Alive TaskData: %llu", AliveTaskDataCount.load());
                ImGui::Text("Total TaskData created: %llu", TaskDataCreatedCount.load());
                ImGui::Text("TaskData created this frame: %llu", taskDataCreatedThisFrame);
            }
            ImGui::End();
        }
    }

    void TaskScheduler::stealJobAndRun(const Async::TaskLane& lane) {
        runSingleTask(lane, false);
    }


    void TaskScheduler::parallelFor(std::size_t count, const std::function<void(std::size_t)>& forEach, std::size_t granularity) {
        if(count == 0) {
            return;
        }

        verify(granularity > 0, "Cannot have a granularity of 0");

        auto run = [&](std::size_t startIndex) {
            for(std::size_t i = startIndex; i < startIndex + granularity && i < count; i++) {
                forEach(i);
            }
        };

        std::size_t parallelJobs = count / granularity; // truncate on purpose, the calling thread will participate
        if(parallelJobs > 0) {
            Async::Counter sync;
            for(std::size_t jobIndex = 0; jobIndex < parallelJobs; jobIndex++) {
                schedule(TaskDescription {
                        .name = "Parallel ForEach",
                        .task = [&, jobIndex](Carrot::TaskHandle&) {
                            std::size_t startIndex = jobIndex * granularity;
                            run(startIndex);
                        },
                        .joiner = &sync,
                }, FrameParallelWork);
            }

            run(parallelJobs * granularity);
            sync.busyWait(); // TODO: potential deadlock if done on thread already running tasks
        } else {
            run(0);
        }
    }

    void TaskScheduler::schedule(TaskDescription&& description, const Async::TaskLane& lane) {
        ZoneScoped;
        verify(lane == TaskScheduler::FrameParallelWork
               || lane == TaskScheduler::AssetLoading
               || lane == TaskScheduler::MainLoop
               || lane == TaskScheduler::Rendering
               , "No other lanes supported at the moment");
        verify(description.task, "No valid task");
        TaskScheduledThisFrameCount++;
        if(description.joiner) {
            description.joiner->increment();
        }

        auto pNewTask = getOrReuseTaskData();
        pNewTask->wantedLane = lane;
        pNewTask->currentLane = lane;
        pNewTask->name = description.name;
        pNewTask->dependency = description.dependency;
        pNewTask->joiner = description.joiner;
        pNewTask->task = description.task;

        {
            auto fiberProc = [pTaskKeepAlive = pNewTask, pTaskScheduler = this](Cider::FiberHandle& fiber) mutable {
                auto pTask = pTaskKeepAlive;
                pTaskKeepAlive = nullptr;

                // fiber prolog
                TaskHandle taskHandle { fiber, *pTask };

                auto* fls = (FiberLocalStorage*) &fiber.localStorage[0];
                fls->taskData = pTask.get();

                ActiveTaskCount++;

                if(pTask->dependency) {
                    taskHandle.wait(*pTask->dependency);
                } else {
                    fiber.yield();
                }

                // execute task
                pTask->task(taskHandle);
                if(pTask->joiner) {
                    pTask->joiner->decrement();
                }

                ActiveTaskCount--;
            };
            pNewTask->fiber->switchToWithOnTop(fiberProc); // execute prolog

            // if task is not waiting on something, start it
            if(!pNewTask->dependency) {
                ZoneScopedN("Push task description to queue");
                taskQueues[lane].push(std::move(pNewTask));
            }
        }
    }

    void TaskScheduler::FiberScheduler::schedule(Cider::FiberHandle& toSchedule) {
        auto& taskData = getTaskData(toSchedule);
        taskScheduler.taskQueues[taskData.wantedLane].push(taskData.shared_from_this());
    }

    void TaskScheduler::FiberScheduler::schedule(Cider::FiberHandle& toSchedule, Cider::Proc proc, void *userData) {
        schedule(toSchedule);
        //proc(userData);
    }

    void TaskScheduler::FiberScheduler::schedule(Cider::FiberHandle& toSchedule, const Cider::StdFunctionProc& proc) {
        schedule(toSchedule);
        //proc();
    }
}