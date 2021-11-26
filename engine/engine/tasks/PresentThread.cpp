//
// Created by jglrxavpok on 17/07/2021.
//

#include "PresentThread.h"
#include "engine/vulkan/CustomTracyVulkan.h"
#include "engine/utils/Macros.h"

namespace Carrot {
    PresentThread::PresentThread(VulkanDriver& driver): driver(driver), presentSemaphore(0) {
      //  backingThread = std::thread([this]() { threadFunc(); });
    }

    void PresentThread::present(std::uint32_t swapchainIndex, const vk::Semaphore& signalSemaphore, const vk::SubmitInfo& submitInfo, const vk::Fence& presentFence) {
        std::lock_guard lk(queueMutex);
        submitQueue.emplace(swapchainIndex, signalSemaphore, submitInfo, presentFence);
        presentSemaphore.release();
    }

    void PresentThread::threadFunc() {
        while(running) {
            presentSemaphore.acquire();
            Submit s;
            {
                std::lock_guard lk(queueMutex);
                s = submitQueue.back();
                submitQueue.pop();
            }

            driver.getGraphicsQueue().submit(s.submitInfo, s.fence);

            vk::SwapchainKHR swapchains[] = { driver.getSwapchain() };

            vk::PresentInfoKHR presentInfo{
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &s.signalSemaphore,

                    .swapchainCount = 1,
                    .pSwapchains = swapchains,
                    .pImageIndices = &s.swapchainIndex,
                    .pResults = nullptr,
            };

            {
                ZoneScopedN("PresentKHR");
                DISCARD(driver.getPresentQueue().presentKHR(&presentInfo));
            }
        }
    }

    PresentThread::~PresentThread() {
        running = false;
      //  backingThread.join();
    }
}