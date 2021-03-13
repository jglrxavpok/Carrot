//
// Created by jglrxavpok on 26/11/2020.
//

#pragma once
#include <vector>
#include "engine/vulkan/includes.h"
#include "engine/Engine.h"

using namespace std;

template<typename CommandBufferConsumer>
void Carrot::Engine::performSingleTimeTransferCommands(CommandBufferConsumer&& consumer) {
    vkDriver.performSingleTimeTransferCommands<CommandBufferConsumer>(consumer);
}

template<typename CommandBufferConsumer>
void Carrot::Engine::performSingleTimeGraphicsCommands(CommandBufferConsumer&& consumer) {
    vkDriver.performSingleTimeGraphicsCommands<CommandBufferConsumer>(consumer);
}
