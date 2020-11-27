//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include "render/Buffer.h"

template<typename T>
void Carrot::Buffer::stageUpload(const std::vector<T>& data) {
    auto stagingBuffer = Buffer(engine, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, {engine.getQueueFamilies().transferFamily.value()});

    auto& device = engine.getLogicalDevice();
    void* pData;
    if(device.mapMemory(*stagingBuffer.memory, 0, size, static_cast<vk::MemoryMapFlags>(0), &pData) != vk::Result::eSuccess) {
        throw runtime_error("Failed to map memory!");
    }
    size_t totalSize = data.size() * sizeof(T);
    memcpy(pData, data.data(), totalSize);
    device.unmapMemory(*stagingBuffer.memory);

    stagingBuffer.copyTo(*this);
}