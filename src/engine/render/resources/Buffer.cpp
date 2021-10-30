//
// Created by jglrxavpok on 27/11/2020.
//

#include "Buffer.h"
#include <set>
#include <vector>
#include "BufferView.h"
#include "ResourceAllocator.h"
#include "engine/Engine.h"
#include <engine/utils/Macros.h>

Carrot::Buffer::Buffer(VulkanDriver& driver, vk::DeviceSize size, vk::BufferUsageFlags usage,
                       vk::MemoryPropertyFlags properties, std::set<uint32_t> families): driver(driver), size(size) {
    auto& queueFamilies = driver.getQueueFamilies();
    if(families.empty()) {
        families.insert(queueFamilies.graphicsFamily.value());
    }

    std::vector<uint32_t> familyList = {families.begin(), families.end()};
    vk::BufferCreateInfo bufferInfo = {
            .size = size,
            .usage = usage,
    };

    if(families.size() == 1) { // same queue for graphics and transfer
        bufferInfo.sharingMode = vk::SharingMode::eExclusive; // used by only one queue
    } else { // separate queues, requires to tell Vulkan which queues
        bufferInfo.sharingMode = vk::SharingMode::eConcurrent; // used by both transfer and graphics queues
        bufferInfo.queueFamilyIndexCount = static_cast<uint32_t>(familyList.size());
        bufferInfo.pQueueFamilyIndices = familyList.data();
    }

    vkBuffer = driver.getLogicalDevice().createBufferUnique(bufferInfo, driver.getAllocationCallbacks());

    vk::MemoryRequirements memoryRequirements = driver.getLogicalDevice().getBufferMemoryRequirements(*vkBuffer);
    vk::StructureChain<vk::MemoryAllocateInfo, vk::MemoryAllocateFlagsInfo> allocInfo = {
            {
                    .allocationSize = memoryRequirements.size,
                    .memoryTypeIndex = driver.findMemoryType(memoryRequirements.memoryTypeBits, properties)
            },
            {
                    .flags = vk::MemoryAllocateFlagBits::eDeviceAddress,
            }
    };

    memory = driver.getLogicalDevice().allocateMemoryUnique(allocInfo.get(), driver.getAllocationCallbacks());
    driver.getLogicalDevice().bindBufferMemory(*vkBuffer, *memory, 0);
}

void Carrot::Buffer::copyTo(Carrot::Buffer& other, vk::DeviceSize offset) const {
    driver.performSingleTimeTransferCommands([&](vk::CommandBuffer &stagingCommands) {
        vk::BufferCopy copyRegion = {
                .srcOffset = 0,
                .dstOffset = offset,
                .size = size,
        };
        stagingCommands.copyBuffer(*vkBuffer, *other.vkBuffer, {copyRegion});
    });
}

void Carrot::Buffer::copyTo(std::span<std::uint8_t> out, vk::DeviceSize offset) const {
    // allocate staging buffer used for transfer
    auto stagingBuffer = Carrot::Buffer(driver, out.size(), vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, std::set<uint32_t>{driver.getQueueFamilies().transferFamily.value()});

    // download data to staging buffer
    driver.performSingleTimeTransferCommands([&](vk::CommandBuffer &stagingCommands) {
        vk::BufferCopy copyRegion = {
                .srcOffset = offset,
                .dstOffset = 0,
                .size = out.size(),
        };
        stagingCommands.copyBuffer(*vkBuffer, *stagingBuffer.vkBuffer, {copyRegion});
    });

    // copy staging buffer to the 'out' buffer
    stagingBuffer.directDownload(out, 0);
}

void Carrot::Buffer::directDownload(std::span<std::uint8_t> out, vk::DeviceSize offset) {
    void* pData;
    if(driver.getLogicalDevice().mapMemory(*memory, offset, out.size(), static_cast<vk::MemoryMapFlags>(0), &pData) != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to map memory!");
    }
    std::memcpy(out.data(), reinterpret_cast<const uint8_t*>(pData), out.size());
    driver.getLogicalDevice().unmapMemory(*memory);
}

const vk::Buffer& Carrot::Buffer::getVulkanBuffer() const {
    return *vkBuffer;
}

void Carrot::Buffer::directUpload(const void* data, vk::DeviceSize length, vk::DeviceSize offset) {
    void* pData;
    if(driver.getLogicalDevice().mapMemory(*memory, offset, length, static_cast<vk::MemoryMapFlags>(0), &pData) != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to map memory!");
    }
    std::memcpy(reinterpret_cast<uint8_t*>(pData), data, length);
    driver.getLogicalDevice().unmapMemory(*memory);
}

uint64_t Carrot::Buffer::getSize() const {
    return size;
}

Carrot::Buffer::~Buffer() {
    if(mappedPtr) {
        unmap();
    }
}

void Carrot::Buffer::setDebugNames(const std::string& name) {
    nameSingle(driver, name, getVulkanBuffer());
}

void Carrot::Buffer::unmap() {
    verify(mappedPtr != nullptr, "Must be mapped!");
    driver.getLogicalDevice().unmapMemory(*memory);
    mappedPtr = nullptr;
}

vk::DeviceAddress Carrot::Buffer::getDeviceAddress() const {
    return driver.getLogicalDevice().getBufferAddress({.buffer = *vkBuffer});
}

Carrot::BufferView Carrot::Buffer::getWholeView() {
    return BufferView(nullptr /* allocator does NOT own this buffer view */, *this, 0u, static_cast<vk::DeviceSize>(size));
}

void Carrot::Buffer::flushMappedMemory(vk::DeviceSize start, vk::DeviceSize length) {
    driver.getLogicalDevice().flushMappedMemoryRanges(vk::MappedMemoryRange {
       .memory = *memory,
       .offset = start,
       .size = length,
    });
}
