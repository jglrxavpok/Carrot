//
// Created by jglrxavpok on 10/03/2021.
//

#include "BufferView.h"
#include "ResourceAllocator.h"

Carrot::BufferView::BufferView(Carrot::ResourceAllocator* allocator, Carrot::Buffer& buffer, vk::DeviceSize start, vk::DeviceSize size):
    allocator(allocator), buffer(buffer), start(start), size(size) {}

vk::DescriptorBufferInfo Carrot::BufferView::asBufferInfo() const {
    return vk::DescriptorBufferInfo {
        .buffer = buffer.getVulkanBuffer(),
        .offset = start,
        .range = size,
    };
}

void Carrot::BufferView::unmap() {
    // TODO: proper segmentation
    buffer.unmap();
}

Carrot::BufferView::~BufferView() {
    if(allocator) {
        allocator->freeBufferView(*this);
    }
}
