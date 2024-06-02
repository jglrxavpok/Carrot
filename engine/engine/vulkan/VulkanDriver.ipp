//
// Created by jglrxavpok on 12/03/2021.
//

#include "VulkanDriver.h"
#include "engine/utils/Macros.h"

template<typename CommandBufferConsumer>
vk::CommandBuffer Carrot::VulkanDriver::recordStandaloneBuffer(vk::CommandPool& commandPool, const CommandBufferConsumer& consumer) {
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
    return stagingCommands;
}

template<typename CommandBufferConsumer>
vk::CommandBuffer Carrot::VulkanDriver::recordStandaloneGraphicsBuffer(const CommandBufferConsumer& consumer) {
    return recordStandaloneBuffer(getThreadGraphicsCommandPool(), consumer);
}

template<typename CommandBufferConsumer>
vk::CommandBuffer Carrot::VulkanDriver::recordStandaloneTransferBuffer(const CommandBufferConsumer& consumer) {
    return recordStandaloneBuffer(getThreadTransferCommandPool(), consumer);
}

template<typename CommandBufferConsumer>
vk::CommandBuffer Carrot::VulkanDriver::recordStandaloneComputeBuffer(const CommandBufferConsumer& consumer) {
    return recordStandaloneBuffer(getThreadComputeCommandPool(), consumer);
}

template<typename CommandBufferConsumer>
void Carrot::VulkanDriver::performSingleTimeCommands(vk::CommandPool& commandPool,
                                                     const std::function<void(const vk::SubmitInfo2&, const vk::Fence&)>& submitAction,
                                                     const std::function<void()>& waitAction,
                                                     bool waitFor,
                                                     vk::Semaphore waitSemaphore,
                                                     vk::PipelineStageFlags2 waitDstFlags,
                                                     vk::Semaphore signalSemaphore,
                                                     CommandBufferConsumer consumer) {
    vk::CommandBuffer stagingCommands = recordStandaloneBuffer(commandPool, consumer);

    vk::CommandBufferSubmitInfo stagingCommandInfo {
        .commandBuffer = stagingCommands
    };

    vk::SubmitInfo2 submitInfo = {
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &stagingCommandInfo,
    };

    vk::SemaphoreSubmitInfo waitInfo;
    vk::SemaphoreSubmitInfo signalInfo;
    if(waitSemaphore) {
        waitInfo.semaphore = waitSemaphore;
        waitInfo.stageMask = waitDstFlags;

        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &waitInfo;
    }
    if(signalSemaphore) {
        signalInfo.semaphore = signalSemaphore;
        signalInfo.stageMask = vk::PipelineStageFlagBits2::eAllCommands;

        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &signalInfo;
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
void Carrot::VulkanDriver::performSingleTimeTransferCommands(CommandBufferConsumer consumer, bool waitFor, vk::Semaphore waitSemaphore, vk::PipelineStageFlags2 waitDstFlags, vk::Semaphore signalSemaphore) {
    performSingleTimeCommands(getThreadTransferCommandPool(), [&](const vk::SubmitInfo2& info, const vk::Fence& fence) { submitTransfer(info, fence); }, waitTransfer, waitFor, waitSemaphore, waitDstFlags, signalSemaphore, consumer);
}

template<typename CommandBufferConsumer>
void Carrot::VulkanDriver::performSingleTimeGraphicsCommands(CommandBufferConsumer consumer, bool waitFor, vk::Semaphore waitSemaphore, vk::PipelineStageFlags2 waitDstFlags, vk::Semaphore signalSemaphore) {
    performSingleTimeCommands(getThreadGraphicsCommandPool(), [&](const vk::SubmitInfo2& info, const vk::Fence& fence) { submitGraphics(info, fence); }, waitGraphics, waitFor, waitSemaphore, waitDstFlags, signalSemaphore, consumer);
}
