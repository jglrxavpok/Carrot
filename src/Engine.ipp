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