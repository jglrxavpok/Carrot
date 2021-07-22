//
// Created by jglrxavpok on 12/03/2021.
//

#include "VulkanDriver.h"


template<typename CommandBufferConsumer>
void Carrot::VulkanDriver::performSingleTimeCommands(vk::CommandPool& commandPool, vk::Queue& queue, bool waitFor, vk::Semaphore waitSemaphore, vk::PipelineStageFlags waitDstFlags, CommandBufferConsumer consumer) {
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
    queue.submit(submitInfo, nullptr);

    if(waitFor) {
        // wait for execution
        queue.waitIdle();

        // free now-useless command buffers
        getLogicalDevice().freeCommandBuffers(commandPool, stagingCommands);
    }
    // TODO: freeCommandBuffer if no wait
}

template<typename CommandBufferConsumer>
void Carrot::VulkanDriver::performSingleTimeTransferCommands(CommandBufferConsumer consumer, bool waitFor, vk::Semaphore waitSemaphore, vk::PipelineStageFlags waitDstFlags) {
    performSingleTimeCommands(getThreadTransferCommandPool(), getTransferQueue(), waitFor, waitSemaphore, waitDstFlags, consumer);
}

template<typename CommandBufferConsumer>
void Carrot::VulkanDriver::performSingleTimeGraphicsCommands(CommandBufferConsumer consumer, bool waitFor, vk::Semaphore waitSemaphore, vk::PipelineStageFlags waitDstFlags) {
    performSingleTimeCommands(getThreadGraphicsCommandPool(), getGraphicsQueue(), waitFor, waitSemaphore, waitDstFlags, consumer);
}
