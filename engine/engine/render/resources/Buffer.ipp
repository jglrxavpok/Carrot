//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include <memory>
#include "Buffer.h"

template<typename... T>
void Carrot::Buffer::stageUpload(const std::vector<T>&... data) {
    uint64_t offset = 0;
    (
            (
                this->stageUpload<T>(std::make_pair<uint64_t, T>(offset, data)),
                offset += data.size() * sizeof(T)
            )
    , ...);
}

template<typename... T>
void Carrot::Buffer::stageUploadWithOffsets(const std::pair<uint64_t, std::vector<T>>&... offsetDataPairs) {
    // allocate staging buffer used for transfer
    auto stagingBuffer = Carrot::Buffer(driver, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, std::set<uint32_t>{driver.getQueueFamilies().transferFamily.value()});

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
void Carrot::Buffer::stageUploadWithOffset(std::uint64_t offset, const T* data, const std::size_t totalLength) {
    // allocate staging buffer used for transfer
    auto stagingBuffer = Carrot::Buffer(driver, totalLength, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, std::set<uint32_t>{driver.getQueueFamilies().transferFamily.value()});

    // upload data to staging buffer
    stagingBuffer.directUpload(data, totalLength);


    // copy staging buffer to this buffer
    stagingBuffer.copyTo(*this, offset);
}

template<typename T>
T* Carrot::Buffer::map() {
    if(mappedPtr) {
       return reinterpret_cast<T*>(mappedPtr);
    }
    void* ptr = driver.getLogicalDevice().mapMemory(*memory, 0, VK_WHOLE_SIZE);
    mappedPtr = ptr;
    return reinterpret_cast<T*>(ptr);
}