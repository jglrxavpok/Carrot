//
// Created by jglrxavpok on 28/12/2021.
//

#include "SingleFrameStackGPUAllocator.h"
#include <engine/Engine.h>
#include <engine/render/resources/BufferView.h>
#include <engine/utils/Macros.h>
#include <core/math/BasicFunctions.h>

namespace Carrot {
    SingleFrameStackGPUAllocator::RingBuffer::RingBuffer(vk::DeviceSize bufferSize, vk::BufferUsageFlags usages, vk::MemoryPropertyFlags memoryProperties): bufferSize(bufferSize), usages(usages), memoryProperties(memoryProperties) {}

    void SingleFrameStackGPUAllocator::RingBuffer::resize(std::size_t length) {
        stackPointers.resize(length);
        std::fill(stackPointers.begin(), stackPointers.end(), 0);

        stacks.clear();
        stacks.resize(length);

        for (std::size_t i = 0; i < length; ++i) {
            stacks[i] = std::make_unique<Carrot::Buffer>(GetVulkanDriver(), bufferSize, usages, memoryProperties);
        }
    }

    void SingleFrameStackGPUAllocator::RingBuffer::clear(std::size_t index) {
        stackPointers[index] = 0;
    }

    Carrot::BufferView SingleFrameStackGPUAllocator::RingBuffer::allocateAligned(std::size_t index, vk::DeviceSize size, vk::DeviceSize align) {
        vk::DeviceSize location = Carrot::Math::alignUp(stackPointers[index], align);

        verify(location+size <= bufferSize, "Out of memory");

        stackPointers[index] = location + size;
        return Carrot::BufferView(nullptr, *stacks[index], location, size);
    }

    vk::DeviceSize SingleFrameStackGPUAllocator::RingBuffer::getAllocatedSize(std::size_t frameIndex) const {
        return stackPointers[frameIndex];
    }

    // --

    SingleFrameStackGPUAllocator::SingleFrameStackGPUAllocator(vk::DeviceSize heapSize):
            totalSizePerBuffer(heapSize),
            buffers(heapSize,
                            vk::BufferUsageFlagBits::eVertexBuffer
                            | vk::BufferUsageFlagBits::eIndexBuffer
                            | vk::BufferUsageFlagBits::eStorageBuffer
                            | vk::BufferUsageFlagBits::eUniformBuffer
                            | vk::BufferUsageFlagBits::eIndirectBuffer
                            | vk::BufferUsageFlagBits::eTransferSrc

                            , vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible) {
        buffers.resize(GetEngine().getSwapchainImageCount());
    }

    void SingleFrameStackGPUAllocator::newFrame(std::size_t frameIndex) {
        currentFrame = frameIndex;
        buffers.clear(currentFrame);
    }

    Carrot::BufferView SingleFrameStackGPUAllocator::allocate(std::size_t size) {
        return buffers.allocateAligned(currentFrame, size);
    }

    vk::DeviceSize SingleFrameStackGPUAllocator::getAllocatedSizeThisFrame() const {
        return buffers.getAllocatedSize(currentFrame);
    }

    vk::DeviceSize SingleFrameStackGPUAllocator::getRemainingSizeThisFrame() const {
        return totalSizePerBuffer - buffers.getAllocatedSize(currentFrame);
    }

    vk::DeviceSize SingleFrameStackGPUAllocator::getAllocatedSizeAllFrames() const {
        vk::DeviceSize sum = 0;
        for (std::size_t frameIndex = 0; frameIndex < GetEngine().getSwapchainImageCount(); ++frameIndex) {
            sum += buffers.getAllocatedSize(frameIndex);
        }
        return sum;
    }

    void SingleFrameStackGPUAllocator::onSwapchainImageCountChange(size_t newCount) {
        buffers.resize(newCount);
    }

    void SingleFrameStackGPUAllocator::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) { /* no-op */ }
}