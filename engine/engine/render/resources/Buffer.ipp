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
                this->stageUpload<T>(std::make_pair<uint64_t, std::vector<T>>(offset, data)),
                offset += data.size() * sizeof(T)
            )
    , ...);
}

template<typename... T>
void Carrot::Buffer::stageUploadWithOffsets(const std::pair<uint64_t, std::span<T>>&... offsetDataPairs) {
    // allocate staging buffer used for transfer
    auto stagingBufferAlloc = internalStagingBuffer(size);
    auto stagingBuffer = stagingBufferAlloc.view;

    // upload data to staging buffer
    (
            (
                    stagingBuffer.subView(offsetDataPairs.first).directUpload(offsetDataPairs.second.data(), offsetDataPairs.second.size() * sizeof(T))
            )
    , ...);


    // copy staging buffer to this buffer
    stagingBuffer.copyToAndWait(getWholeView());
}

template<typename T>
void Carrot::Buffer::stageUploadWithOffsets(const std::span<std::pair<uint64_t, std::span<T>>>& offsetDataPairs) {
    // allocate staging buffer used for transfer
    auto stagingBufferAlloc = internalStagingBuffer(size);
    auto stagingBuffer = stagingBufferAlloc.view;

    // upload data to staging buffer
    for(const auto& [offset, data] : offsetDataPairs) {
        stagingBuffer.directUpload(data.data(), data.size() * sizeof(T), offset);
    }

    // copy staging buffer to this buffer
    stagingBuffer.copyToAndWait(getWholeView());
}

template<typename T>
void Carrot::Buffer::stageUploadWithOffset(std::uint64_t offset, const T* data, const std::size_t totalLength) {
    // allocate staging buffer used for transfer
    auto stagingBufferAlloc = internalStagingBuffer(totalLength);
    auto stagingBuffer = stagingBufferAlloc.view;

    // upload data to staging buffer
    stagingBuffer.directUpload(data, totalLength);

    // copy staging buffer to this buffer
    stagingBuffer.copyToAndWait(getWholeView().subView(offset));
}

template<typename T>
void Carrot::Buffer::stageAsyncUploadWithOffset(vk::Semaphore& semaphore, std::uint64_t offset, const T* data, const std::size_t totalLength) {
    // allocate staging buffer used for transfer
    heldStagingBuffer = internalStagingBuffer(totalLength);
    auto stagingBuffer = heldStagingBuffer.view;

    // upload data to staging buffer
    stagingBuffer.directUpload(data, totalLength);

    // copy staging buffer to this buffer
    stagingBuffer.copyTo(semaphore, getWholeView().subView(offset, totalLength));
}

template<typename T>
void Carrot::Buffer::cmdStageCopy(vk::CommandBuffer& cmds, uint64_t offset, const T* data, std::size_t totalLength) {
    // allocate staging buffer used for transfer
    heldStagingBuffer = internalStagingBuffer(totalLength);
    auto stagingBuffer = heldStagingBuffer.view;

    // upload data to staging buffer
    stagingBuffer.directUpload(data, totalLength);

    // copy staging buffer to this buffer
    stagingBuffer.cmdCopyTo(cmds, getWholeView().subView(offset, totalLength));
}

template<typename T>
T* Carrot::Buffer::map() {
    return reinterpret_cast<T*>(mapGeneric());
}