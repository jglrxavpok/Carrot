//
// Created by jglrxavpok on 26/11/2020.
//

#pragma once
#include <vector>
#include "engine/vulkan/includes.h"
#include "engine/Engine.h"

template<typename CommandBufferConsumer>
void Carrot::Engine::performSingleTimeTransferCommands(CommandBufferConsumer&& consumer, bool waitFor, vk::Semaphore waitSemaphore, vk::PipelineStageFlags waitDstFlags, vk::Semaphore signalSemaphore) {
    vkDriver.performSingleTimeTransferCommands<CommandBufferConsumer>(consumer, waitFor, waitSemaphore, waitDstFlags, signalSemaphore);
}

template<typename CommandBufferConsumer>
void Carrot::Engine::performSingleTimeGraphicsCommands(CommandBufferConsumer&& consumer, bool waitFor, vk::Semaphore waitSemaphore, vk::PipelineStageFlags waitDstFlags, vk::Semaphore signalSemaphore) {
    vkDriver.performSingleTimeGraphicsCommands<CommandBufferConsumer>(consumer, waitFor, waitSemaphore, waitDstFlags, signalSemaphore);
}
