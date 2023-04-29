//
// Created by jglrxavpok on 10/03/2021.
//

#include "ResourceAllocator.h"
#include <engine/Engine.h>

static vk::DeviceSize HeapSize = 32 * 1024 * 1024; // 32MB, no particular reason for this exact value

Carrot::ResourceAllocator::ResourceAllocator(Carrot::VulkanDriver& device): device(device) {}

Carrot::BufferView Carrot::ResourceAllocator::allocateStagingBuffer(vk::DeviceSize size) {
    if(size > HeapSize) {
        dedicatedStagingBuffers.emplace_back(allocateDedicatedBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, std::set<uint32_t>{GetVulkanDriver().getQueueFamilies().transferFamily.value()}));
        return dedicatedStagingBuffers.back()->getWholeView();
    }

    for(auto& heap : stagingHeaps) {
        if(heap.getRemainingSizeThisFrame() >= size) {
            return heap.allocate(size);
        }
    }

    // no heap with enough memory, create a new one
    stagingHeaps.emplace_back(HeapSize);
    return stagingHeaps.back().allocate(size);
}

void Carrot::ResourceAllocator::beginFrame(const Carrot::Render::Context& renderContext) {
    for(auto& heap : stagingHeaps) {
        heap.newFrame(renderContext.swapchainIndex);
    }

    // the hope is that such buffers are only for mesh/texture uploads, which should be punctual events
    dedicatedStagingBuffers.clear();
}

Carrot::BufferView Carrot::ResourceAllocator::allocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                                             vk::MemoryPropertyFlags properties,
                                                             const std::set<uint32_t>& families) {
    // TODO: proper allocation algorithm
    auto buffer = allocateDedicatedBuffer(size, usage, properties, families);
    auto result = buffer->getWholeView();
    allocatedBuffers.emplace_back(move(buffer));
    return result;
}

std::unique_ptr<Carrot::Buffer> Carrot::ResourceAllocator::allocateDedicatedBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                                                      vk::MemoryPropertyFlags properties,
                                                                      const std::set<uint32_t>& families) {
    return std::make_unique<Buffer>(device, size, usage, properties, families);
}

void Carrot::ResourceAllocator::freeBufferView(Carrot::BufferView& view) {
    // TODO: proper allocation algorithm
    std::erase_if(allocatedBuffers, [&](const auto& p) { return p->getVulkanBuffer() == view.getBuffer().getVulkanBuffer(); });
}
