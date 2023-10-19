//
// Created by jglrxavpok on 10/03/2021.
//

#include "BufferView.h"
#include "ResourceAllocator.h"
#include "engine/utils/Macros.h"
#include "engine/Engine.h"
#include "Buffer.h"

Carrot::BufferView::BufferView(Carrot::ResourceAllocator* allocator, Carrot::Buffer& buffer, vk::DeviceSize start, vk::DeviceSize size):
    allocator(allocator), buffer(&buffer), start(start), size(size) {}

vk::DescriptorBufferInfo Carrot::BufferView::asBufferInfo() const {
    return vk::DescriptorBufferInfo {
        .buffer = getBuffer().getVulkanBuffer(),
        .offset = start,
        .range = size,
    };
}

void* Carrot::BufferView::mapGeneric() {
    return getBuffer().map<void>();
}

void Carrot::BufferView::unmap() {
    // TODO: proper segmentation
    getBuffer().unmap();
}

void Carrot::BufferView::flushMappedMemory() {
    getBuffer().flushMappedMemory(start, size);
}

bool Carrot::BufferView::operator==(const Carrot::BufferView& other) const {
    return start == other.start && size == other.size && buffer == other.buffer;
}

const vk::Buffer& Carrot::BufferView::getVulkanBuffer() const {
    return getBuffer().getVulkanBuffer();
}

Carrot::BufferView Carrot::BufferView::subView(std::size_t substart) const {
    verify(substart <= size, "Out-of-bounds subview");
    return BufferView {
        allocator,
        *buffer,
        start + substart,
        size - substart,
    };
}

Carrot::BufferView Carrot::BufferView::subView(std::size_t substart, std::size_t subcount) const {
    verify(substart+subcount <= size, "subview exceeds original buffer");
    return BufferView {
        allocator,
        *buffer,
        start + substart,
        subcount
    };
}

void Carrot::BufferView::directUpload(const void* data, vk::DeviceSize length, vk::DeviceSize offset) {
    verify(length <= size, "Cannot upload more data than this view allows");
    getBuffer().directUpload(data, length, start+offset);
}

void Carrot::BufferView::stageUpload(const void* data, vk::DeviceSize length, vk::DeviceSize offset) {
    verify(length <= size, "Cannot upload more data than this view allows");
    getBuffer().stageUploadWithOffset(start+offset, data, length);
}

void Carrot::BufferView::copyToAndWait(Carrot::BufferView destination) const {
    verify(destination.size >= size, "copying too much data");
    GetVulkanDriver().performSingleTimeTransferCommands([&](vk::CommandBuffer &stagingCommands) {
        vk::BufferCopy copyRegion = {
                .srcOffset = start,
                .dstOffset = destination.start,
                .size = size,
        };
        stagingCommands.copyBuffer(getVulkanBuffer(), destination.getVulkanBuffer(), {copyRegion});
    });
}

void Carrot::BufferView::copyTo(vk::Semaphore& signalSemaphore, Carrot::BufferView destination) const {
    verify(destination.size >= size, "copying too much data");
    GetVulkanDriver().performSingleTimeTransferCommands([&](vk::CommandBuffer &stagingCommands) {
        cmdCopyTo(stagingCommands, destination);
    }, false, {}, static_cast<vk::PipelineStageFlagBits>(0), signalSemaphore);
}

void Carrot::BufferView::cmdCopyTo(vk::CommandBuffer& cmds, Carrot::BufferView destination) const {
    vk::BufferCopy copyRegion = {
            .srcOffset = start,
            .dstOffset = destination.start,
            .size = size,
    };
    cmds.copyBuffer(getVulkanBuffer(), destination.getVulkanBuffer(), {copyRegion});
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

vk::DeviceAddress Carrot::BufferView::getDeviceAddress() const {
    return buffer->getDeviceAddress() + start;
}

Carrot::BufferView::~BufferView() {
    if(allocator) {
        allocator->freeBufferView(*this);
    }
}
