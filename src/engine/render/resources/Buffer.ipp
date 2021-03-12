//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include <memory>
#include "Buffer.h"

template<typename... T>
void Carrot::Buffer::stageUpload(const vector<T>&... data) {
    uint64_t offset = 0;
    (
            (
                this->stageUpload<T>(make_pair<uint64_t, T>(offset, data)),
                offset += data.size() * sizeof(T)
            )
    , ...);
}

template<typename... T>
void Carrot::Buffer::stageUploadWithOffsets(const pair<uint64_t, vector<T>>&... offsetDataPairs) {
    // allocate staging buffer used for transfer
    auto stagingBuffer = Carrot::Buffer(device, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, set<uint32_t>{device.getQueueFamilies().transferFamily.value()});

    // upload data to staging buffer
    (
            (
                    stagingBuffer.directUpload(offsetDataPairs.second.data(), offsetDataPairs.second.size() * sizeof(T), offsetDataPairs.first)
            )
    , ...);


    // copy staging buffer to this buffer
    stagingBuffer.copyTo(*this, 0);

    // stagingBuffer will be destroyed, and resources freed after this point
}

template<typename T>
void Carrot::Buffer::stageUploadWithOffset(uint64_t offset, const T* data) {
    // allocate staging buffer used for transfer
    auto stagingBuffer = Carrot::Buffer(device, sizeof(T), vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, set<uint32_t>{device.getQueueFamilies().transferFamily.value()});

    // upload data to staging buffer
    stagingBuffer.directUpload(data, sizeof(T));


    // copy staging buffer to this buffer
    stagingBuffer.copyTo(*this, offset);
}

template<typename T>
T* Carrot::Buffer::map() {
    void* ptr = device.getLogicalDevice().mapMemory(*memory, 0, VK_WHOLE_SIZE);
    return reinterpret_cast<T*>(ptr);
}