//
// Created by jglrxavpok on 12/03/2021.
//

#include "VulkanDriver.h"


template<typename CommandBufferConsumer>
void Carrot::VulkanDriver::performSingleTimeCommands(vk::CommandPool& commandPool, vk::Queue& queue, CommandBufferConsumer consumer) {
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
    queue.submit(submitInfo, nullptr);

    // wait for execution
    queue.waitIdle();

    // free now-useless command buffers
    getLogicalDevice().freeCommandBuffers(commandPool, stagingCommands);
}

template<typename CommandBufferConsumer>
void Carrot::VulkanDriver::performSingleTimeTransferCommands(CommandBufferConsumer consumer) {
    performSingleTimeCommands(getThreadTransferCommandPool(), getTransferQueue(), consumer);
}

template<typename CommandBufferConsumer>
void Carrot::VulkanDriver::performSingleTimeGraphicsCommands(CommandBufferConsumer consumer) {
    performSingleTimeCommands(getThreadGraphicsCommandPool(), getGraphicsQueue(), consumer);
}
