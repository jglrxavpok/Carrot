//
// Created by jglrxavpok on 12/03/2021.
//

#include "VulkanDriver.h"
#include "engine/utils/Macros.h"

template<typename CommandBufferConsumer>
void Carrot::VulkanDriver::performSingleTimeCommands(vk::CommandPool& commandPool,
                                                     const std::function<void(const vk::SubmitInfo&, const vk::Fence&)>& submitAction,
                                                     const std::function<void()>& waitAction,
                                                     bool waitFor,
                                                     vk::Semaphore waitSemaphore,
                                                     vk::PipelineStageFlags waitDstFlags,
                                                     vk::Semaphore signalSemaphore,
                                                     CommandBufferConsumer consumer) {
    // allocate command buffer
    vk::CommandBufferAllocateInfo allocationInfo {
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
    };
    vk::CommandBuffer stagingCommands = getLogicalDevice().allocateCommandBuffers(allocationInfo)[0];

    // begin recording
    vk::CommandBufferBeginInfo beginInfo {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    stagingCommands.begin(beginInfo);

    {
        // let user record their commands
        consumer(stagingCommands);
    }

    // end and submit
    stagingCommands.end();

    vk::SubmitInfo submitInfo = {
            .commandBufferCount = 1,
            .pCommandBuffers = &stagingCommands,
    };

    if(waitSemaphore) {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitSemaphore;
        submitInfo.pWaitDstStageMask = &waitDstFlags;
    }
    if(signalSemaphore) {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalSemaphore;
    }
    submitAction(submitInfo, nullptr);

    if(waitFor) {
        // wait for execution
        waitAction();

        // free now-useless command buffers
        getLogicalDevice().freeCommandBuffers(commandPool, stagingCommands);
    } else {
        deferCommandBufferDestruction(commandPool, stagingCommands);
    }
}

template<typename CommandBufferConsumer>
void Carrot::VulkanDriver::performSingleTimeTransferCommands(CommandBufferConsumer consumer, bool waitFor, vk::Semaphore waitSemaphore, vk::PipelineStageFlags waitDstFlags, vk::Semaphore signalSemaphore) {
    performSingleTimeCommands(getThreadTransferCommandPool(), [&](const vk::SubmitInfo& info, const vk::Fence& fence) { submitTransfer(info, fence); }, waitTransfer, waitFor, waitSemaphore, waitDstFlags, signalSemaphore, consumer);
}

template<typename CommandBufferConsumer>
void Carrot::VulkanDriver::performSingleTimeGraphicsCommands(CommandBufferConsumer consumer, bool waitFor, vk::Semaphore waitSemaphore, vk::PipelineStageFlags waitDstFlags, vk::Semaphore signalSemaphore) {
    performSingleTimeCommands(getThreadGraphicsCommandPool(), [&](const vk::SubmitInfo& info, const vk::Fence& fence) { submitGraphics(info, fence); }, waitGraphics, waitFor, waitSemaphore, waitDstFlags, signalSemaphore, consumer);
}
