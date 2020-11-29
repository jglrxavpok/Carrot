//
// Created by jglrxavpok on 26/11/2020.
//

#pragma once
#include <vector>
#include "vulkan/includes.h"
#include "Engine.h"

using namespace std;

template<typename T>
void Carrot::Engine::uploadBuffer(vk::Device& device, vk::Buffer& buffer, vk::DeviceMemory& memory, const vector<T>& data) {
    size_t fullSize = data.size() * sizeof(T);
    void* pData;
    device.mapMemory(memory, 0, fullSize, static_cast<vk::MemoryMapFlagBits>(0), &pData);
    memcpy(pData, data.data(), fullSize);
    device.unmapMemory(memory);
}

template<typename CommandBufferConsumer>
void Carrot::Engine::performSingleTimeCommands(vk::CommandPool& commandPool, vk::Queue& queue, CommandBufferConsumer consumer) {
    vk::CommandBufferAllocateInfo allocationInfo {
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
    };
    vk::CommandBuffer stagingCommands = device->allocateCommandBuffers(allocationInfo)[0];

    vk::CommandBufferBeginInfo beginInfo {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    stagingCommands.begin(beginInfo);

    consumer(stagingCommands);

    stagingCommands.end();

    vk::SubmitInfo submitInfo = {
            .commandBufferCount = 1,
            .pCommandBuffers = &stagingCommands,
    };
    queue.submit(submitInfo, nullptr);
    queue.waitIdle();

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