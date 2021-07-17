//
// Created by jglrxavpok on 17/07/2021.
//

#pragma once

#include "engine/vulkan/VulkanDriver.h"
#include <queue>
#include <semaphore>

namespace Carrot {
    /// Thread responsible for submitting command buffers to Vulkan
    class PresentThread {
    public:
        void present(std::uint32_t swapchainIndex, const vk::Semaphore& signalSemaphore, const vk::SubmitInfo& submitInfo, const vk::Fence& presentFence);

    private:
        struct Submit {
            std::uint32_t swapchainIndex;
            vk::Semaphore signalSemaphore;
            vk::SubmitInfo submitInfo;
            vk::Fence fence;
        };

        VulkanDriver& driver;
        std::counting_semaphore<100> presentSemaphore;
        std::thread backingThread;

        std::mutex queueMutex;
        std::queue<Submit> submitQueue;
        bool running = true;

        explicit PresentThread(VulkanDriver& driver);
        ~PresentThread();

        void threadFunc();

        friend class Engine;
    };
}
