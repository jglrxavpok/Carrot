//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include <memory>
#include "render/Buffer.h"

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
    auto stagingBuffer = Carrot::Buffer(engine, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, set<uint32_t>{engine.getQueueFamilies().transferFamily.value()});

    // upload data to staging buffer
    (
            (
                    stagingBuffer.directUpload(offsetDataPairs.second.data(), offsetDataPairs.second.size() * sizeof(T), offsetDataPairs.first)
            )
    , ...);


    // copy staging buffer to this buffer
    stagingBuffer.copyTo(*this);

    // stagingBuffer will be destroyed, and resources freed after this point
}

template<typename T>
void Carrot::Buffer::stageUploadWithOffset(uint64_t offset, const T* data) {
    // allocate staging buffer used for transfer
    auto stagingBuffer = Carrot::Buffer(engine, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, set<uint32_t>{engine.getQueueFamilies().transferFamily.value()});

    // upload data to staging buffer
    stagingBuffer.directUpload(data, sizeof(T), offset);


    // copy staging buffer to this buffer
    stagingBuffer.copyTo(*this);
}