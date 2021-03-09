//
// Created by jglrxavpok on 26/11/2020.
//

#pragma once
#include <vector>
#include "engine/vulkan/includes.h"
#include "engine/Engine.h"

using namespace std;

template<typename CommandBufferConsumer>
void Carrot::Engine::performSingleTimeCommands(vk::CommandPool& commandPool, vk::Queue& queue, CommandBufferConsumer consumer) {
    // allocate command buffer
    vk::CommandBufferAllocateInfo allocationInfo {
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
    };
    vk::CommandBuffer stagingCommands = device->allocateCommandBuffers(allocationInfo)[0];

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
    device->freeCommandBuffers(commandPool, stagingCommands);
}

template<typename CommandBufferConsumer>
void Carrot::Engine::performSingleTimeTransferCommands(CommandBufferConsumer consumer) {
    performSingleTimeCommands(*transferCommandPool, transferQueue, consumer);
}

template<typename CommandBufferConsumer>
void Carrot::Engine::performSingleTimeGraphicsCommands(CommandBufferConsumer consumer) {
    performSingleTimeCommands(*graphicsCommandPool, graphicsQueue, consumer);
}
