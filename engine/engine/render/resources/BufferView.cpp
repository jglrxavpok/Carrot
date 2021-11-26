//
// Created by jglrxavpok on 10/03/2021.
//

#include "BufferView.h"
#include "ResourceAllocator.h"

Carrot::BufferView::BufferView(Carrot::ResourceAllocator* allocator, Carrot::Buffer& buffer, vk::DeviceSize start, vk::DeviceSize size):
    allocator(allocator), buffer(&buffer), start(start), size(size) {}

vk::DescriptorBufferInfo Carrot::BufferView::asBufferInfo() const {
    return vk::DescriptorBufferInfo {
        .buffer = getBuffer().getVulkanBuffer(),
        .offset = start,
        .range = size,
    };
}

void Carrot::BufferView::unmap() {
    // TODO: proper segmentation
    getBuffer().unmap();
}

void Carrot::BufferView::flushMappedMemory() {
    getBuffer().flushMappedMemory(start, size);
}

Carrot::BufferView::~BufferView() {
    if(allocator) {
        allocator->freeBufferView(*this);
    }
}
