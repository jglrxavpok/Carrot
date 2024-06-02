//
// Created by jglrxavpok on 05/02/2022.
//

#pragma once

#include "includes.h"
#include <mutex>

#include <engine/vulkan/DebugNameable.h>

namespace Carrot::Vulkan {
    class SynchronizedQueue: public DebugNameable {
    public:
        SynchronizedQueue() = default;
        SynchronizedQueue(vk::Queue queue);

        void waitIdle();

        void submit(const vk::SubmitInfo& info, const vk::Fence& fence = {});
        void submit(const vk::SubmitInfo2& info, const vk::Fence& fence = {});
        void presentKHR(const vk::PresentInfoKHR& info);

        /// Gets the mutex for this queue, can be used to synchronize when only the vk::Queue is supported in a function call
        std::mutex& getMutexUnsafe();

        /// Gets this queue, can be used when only the vk::Queue is supported in a function call
        vk::Queue& getQueueUnsafe();

    public:
        SynchronizedQueue& operator=(const SynchronizedQueue& toCopy);

    protected:
        void setDebugNames(const std::string &name) override;

    private:
        vk::Queue queue;
        std::mutex mutex;
    };
}
