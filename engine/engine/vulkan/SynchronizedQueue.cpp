//
// Created by jglrxavpok on 05/02/2022.
//

#include "SynchronizedQueue.h"

#include <core/io/Logging.hpp>

#include "engine/utils/Macros.h"
#include "engine/Engine.h"
#include "engine/vulkan/VulkanDriver.h"

namespace Carrot::Vulkan {
    static Carrot::Log::Category debugCategory { "Synchronisation debug" };

    SynchronizedQueue::SynchronizedQueue(vk::Queue queue): queue(queue) {}

    void SynchronizedQueue::waitIdle() {
        std::lock_guard l0 { GetVulkanDriver().getDeviceMutex() };
        std::lock_guard l { mutex };
        try {
            queue.waitIdle();
        } catch(vk::DeviceLostError& deviceLost) {
            GetVulkanDriver().onDeviceLost();
        }
    }


    static std::string makeWaitingSemaphoreList(const vk::SubmitInfo& info) {
        if (info.waitSemaphoreCount > 0) {
            std::string s = "";
            for (u32 i = 0; i < info.waitSemaphoreCount; i++) {
                s += Carrot::sprintf("%llx (%s), ", info.pWaitSemaphores[i], DebugNameable::objectNames[vk::Semaphore::objectType][(u64)(VkSemaphore)(info.pWaitSemaphores[i])].c_str());
            }
            return s;
        }
        return "";
    }

    static std::string makeSignalSemaphoreList(const vk::SubmitInfo& info) {
        if (info.signalSemaphoreCount > 0) {
            std::string s = "";
            for (u32 i = 0; i < info.signalSemaphoreCount; i++) {
                s += Carrot::sprintf("%llx (%s), ", info.pSignalSemaphores[i], DebugNameable::objectNames[vk::Semaphore::objectType][(u64)(VkSemaphore)(info.pSignalSemaphores[i])].c_str());
            }
            return s;
        }
        return "";
    }

    static std::string makeSignalSemaphoreList(const vk::SubmitInfo2& info) {
        if (info.signalSemaphoreInfoCount > 0) {
            std::string s = "";
            for (u32 i = 0; i < info.signalSemaphoreInfoCount; i++) {
                s += Carrot::sprintf("%llx (%s), ", info.pSignalSemaphoreInfos[i].semaphore, DebugNameable::objectNames[vk::Semaphore::objectType][(u64)(VkSemaphore)(info.pSignalSemaphoreInfos[i].semaphore)].c_str());
            }
            return s;
        }
        return "";
    }

    static std::string makeWaitingSemaphoreList(const vk::SubmitInfo2& info) {
        if (info.waitSemaphoreInfoCount > 0) {
            std::string s = "";
            for (u32 i = 0; i < info.waitSemaphoreInfoCount; i++) {
                s += Carrot::sprintf("%llx (%s), ", info.pWaitSemaphoreInfos[i].semaphore, DebugNameable::objectNames[vk::Semaphore::objectType][(u64)(VkSemaphore)(info.pWaitSemaphoreInfos[i].semaphore)].c_str());
            }
            return s;
        }
        return "";
    }

    void SynchronizedQueue::submit(const vk::SubmitInfo& info, const vk::Fence& fence) {
        std::lock_guard l0 { GetVulkanDriver().getDeviceMutex() };
        std::lock_guard l { mutex };
        try {
            /*std::string waitSemaphoreList = makeWaitingSemaphoreList(info);
            std::string signalSemaphoreList = makeSignalSemaphoreList(info);
            if (!waitSemaphoreList.empty()) {
                Carrot::Log::debug(debugCategory, "Queue %llx is waiting on semaphores: %s", (u64)this, waitSemaphoreList.c_str());
                Carrot::Log::flush();
            }
            if (!signalSemaphoreList.empty()) {
                Carrot::Log::debug(debugCategory, "Queue %llx is signaling semaphores: %s", (u64)this, signalSemaphoreList.c_str());
                Carrot::Log::flush();
            }*/
            queue.submit(info, fence);
        } catch(vk::DeviceLostError& deviceLost) {
            GetVulkanDriver().onDeviceLost();
        }
    }

    void SynchronizedQueue::submit(const vk::SubmitInfo2& info, const vk::Fence& fence) {
        {
        std::lock_guard l0 { GetVulkanDriver().getDeviceMutex() };
        std::lock_guard l { mutex };
        try {
            /*std::string waitSemaphoreList = makeWaitingSemaphoreList(info);
            std::string signalSemaphoreList = makeSignalSemaphoreList(info);
            if (!waitSemaphoreList.empty()) {
                Carrot::Log::debug(debugCategory, "Queue %llx is waiting on semaphores: %s", (u64)this, waitSemaphoreList.c_str());
                Carrot::Log::flush();
            }
            if (!signalSemaphoreList.empty()) {
                Carrot::Log::debug(debugCategory, "Queue %llx is signaling semaphores: %s", (u64)this, signalSemaphoreList.c_str());
                Carrot::Log::flush();
            }
*/
            queue.submit2(info, fence);
        } catch(vk::DeviceLostError& deviceLost) {
            GetVulkanDriver().onDeviceLost();
        }
        }
        WaitDeviceIdle(); // TODO GMTK replicate RADV_DEBUG=hang which does not crash :c

    }

    void SynchronizedQueue::presentKHR(const vk::PresentInfoKHR& info) {
        std::lock_guard l0 { GetVulkanDriver().getDeviceMutex() };
        std::lock_guard l { mutex };
        DISCARD(queue.presentKHR(&info));
    }

    std::mutex& SynchronizedQueue::getMutexUnsafe() {
        return mutex;
    }

    vk::Queue& SynchronizedQueue::getQueueUnsafe() {
        return queue;
    }

    SynchronizedQueue& SynchronizedQueue::operator=(const SynchronizedQueue& toCopy) {
        queue = toCopy.queue;
        return *this;
    }

    void SynchronizedQueue::setDebugNames(const std::string &name) {
        nameSingle(name, queue);
    }
}