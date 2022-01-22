//
// Created by jglrxavpok on 10/03/2021.
//

#include "BufferView.h"
#include "ResourceAllocator.h"
#include "engine/utils/Macros.h"
#include "engine/Engine.h"

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

bool Carrot::BufferView::operator==(const Carrot::BufferView& other) const {
    return start == other.start && size == other.size && getVulkanBuffer() == other.getVulkanBuffer();
}

void Carrot::BufferView::directUpload(const void* data, vk::DeviceSize length) {
    verify(length <= size, "Cannot upload more data than this view allows");
    getBuffer().directUpload(data, length, start);
}

void Carrot::BufferView::download(const std::span<std::uint8_t>& data, std::uint32_t offset) const {
    verify(offset >= 0, "Offset must be >= 0");
    verify(offset < size, "Offset must be < size");
    verify(data.size() <= size - offset, "Cannot upload more data than this view allows");

    if(getBuffer().isDeviceLocal()) {
        Carrot::Buffer tmpBuffer { GetVulkanDriver(), data.size(), vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible };
        buffer->copyTo(tmpBuffer, offset + start, 0);
        tmpBuffer.directDownload(data, 0);
    } else {
        buffer->directDownload(data, offset + start);
    }
}

Carrot::BufferView::~BufferView() {
    if(allocator) {
        allocator->freeBufferView(*this);
    }
}
