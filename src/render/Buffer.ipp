//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once
#include "render/Buffer.h"

template<typename T>
void Carrot::Buffer::stageUpload(const std::vector<T>& data) {
    auto stagingBuffer = Buffer(engine, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, {engine.getQueueFamilies().transferFamily.value()});

    auto& device = engine.getLogicalDevice();
    stagingBuffer.directUpload(data.data(), data.size() * sizeof(T));

    stagingBuffer.copyTo(*this);
}