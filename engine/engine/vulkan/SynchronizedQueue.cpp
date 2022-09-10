//
// Created by jglrxavpok on 05/02/2022.
//

#include "SynchronizedQueue.h"
#include "engine/utils/Macros.h"
#include "engine/Engine.h"
#include "engine/vulkan/VulkanDriver.h"

namespace Carrot::Vulkan {
    SynchronizedQueue::SynchronizedQueue(vk::Queue queue): queue(queue) {}

    void SynchronizedQueue::waitIdle() {
        std::lock_guard l0 { GetVulkanDriver().getDeviceMutex() };
        std::lock_guard l { mutex };
        queue.waitIdle();
    }

    void SynchronizedQueue::submit(const vk::SubmitInfo& info, const vk::Fence& fence) {
        std::lock_guard l0 { GetVulkanDriver().getDeviceMutex() };
        std::lock_guard l { mutex };
        try {
            queue.submit(info, fence);
        } catch(vk::DeviceLostError& deviceLost) {
            GetVulkanDriver().onDeviceLost();
        }
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
}