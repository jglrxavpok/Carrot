//
// Created by jglrxavpok on 10/03/2021.
//

#include "ResourceAllocator.h"

Carrot::ResourceAllocator::ResourceAllocator(Carrot::VulkanDriver& device): device(device) {}

Carrot::BufferView Carrot::ResourceAllocator::allocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                                             vk::MemoryPropertyFlags properties,
                                                             const std::set<uint32_t>& families) {
    // TODO: proper allocation algorithm
    auto buffer = allocateDedicatedBuffer(size, usage, properties, families);
    auto result = buffer->getWholeView();
    allocatedBuffers.emplace_back(move(buffer));
    return result;
}

unique_ptr<Carrot::Buffer> Carrot::ResourceAllocator::allocateDedicatedBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                                                      vk::MemoryPropertyFlags properties,
                                                                      const std::set<uint32_t>& families) {
    return make_unique<Buffer>(device, size, usage, properties, families);
}

void Carrot::ResourceAllocator::freeBufferView(Carrot::BufferView& view) {
    // TODO: proper allocation algorithm
    allocatedBuffers.erase(find_if(allocatedBuffers.begin(), allocatedBuffers.end(), [&](const auto& p) { return p->getVulkanBuffer() == view.getBuffer().getVulkanBuffer(); }));
}
