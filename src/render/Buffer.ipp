//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include <memory>
#include "render/Buffer.h"

template<typename T>
void Carrot::Buffer::stageUpload(const std::vector<T>& data) {
    // allocate staging buffer used for transfer
    auto stagingBuffer = Carrot::Buffer(engine, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, set<uint32_t>{engine.getQueueFamilies().transferFamily.value()});

    // upload data to staging buffer
    auto& device = engine.getLogicalDevice();
    stagingBuffer.directUpload(data.data(), data.size() * sizeof(T));

    // copy staging buffer to this buffer
    stagingBuffer.copyTo(*this);

    // stagingBuffer will be destroyed, and resources freed after this point
}