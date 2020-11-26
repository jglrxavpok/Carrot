//
// Created by jglrxavpok on 26/11/2020.
//

#pragma once
#include <vector>
#include "vulkan/includes.h"

using namespace std;

template<typename T>
vk::UniqueDeviceMemory CarrotEngine::allocateUploadBuffer(vk::Device& device, vk::Buffer& buffer, const vector<T>& data) {
    vk::MemoryRequirements memoryRequirements = device.getBufferMemoryRequirements(buffer);
    vk::MemoryAllocateInfo allocInfo = {
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
    };

    vk::UniqueDeviceMemory memory = device.allocateMemoryUnique(allocInfo, allocator);
    device.bindBufferMemory(buffer, *memory, 0);

    size_t fullSize = data.size() * sizeof(T);
    void* pData;
    device.mapMemory(*memory, 0, fullSize, static_cast<vk::MemoryMapFlagBits>(0), &pData);
    memcpy(pData, data.data(), fullSize);
    device.unmapMemory(*memory);

    return std::move(memory);
}