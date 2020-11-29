//
// Created by jglrxavpok on 27/11/2020.
//

#include "Buffer.h"
#include <set>
#include <vector>
#include <Engine.h>

Carrot::Buffer::Buffer(Engine& engine, vk::DeviceSize size, vk::BufferUsageFlags usage,
                       vk::MemoryPropertyFlags properties, std::set<uint32_t> families): engine(engine), size(size) {
    auto& device = engine.getLogicalDevice();
    auto& queueFamilies = engine.getQueueFamilies();
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

    vkBuffer = device.createBufferUnique(bufferInfo, engine.getAllocator());

    vk::MemoryRequirements memoryRequirements = device.getBufferMemoryRequirements(*vkBuffer);
    vk::MemoryAllocateInfo allocInfo = {
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = engine.findMemoryType(memoryRequirements.memoryTypeBits, properties)
    };

    memory = device.allocateMemoryUnique(allocInfo, engine.getAllocator());
    device.bindBufferMemory(*vkBuffer, *memory, 0);
}

void Carrot::Buffer::copyTo(Carrot::Buffer& other) const {
    engine.performSingleTimeTransferCommands([&](vk::CommandBuffer &stagingCommands) {
        vk::BufferCopy copyRegion = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = size,
        };
        stagingCommands.copyBuffer(*vkBuffer, *other.vkBuffer, {copyRegion});
    });
}

const vk::Buffer& Carrot::Buffer::getVulkanBuffer() const {
    return *vkBuffer;
}

void Carrot::Buffer::directUpload(const void* data, size_t length) {
    auto& device = engine.getLogicalDevice();
    void* pData;
    if(device.mapMemory(*memory, 0, size, static_cast<vk::MemoryMapFlags>(0), &pData) != vk::Result::eSuccess) {
        throw runtime_error("Failed to map memory!");
    }
    memcpy(pData, data, length);
    device.unmapMemory(*memory);
}

uint64_t Carrot::Buffer::getSize() const {
    return size;
}
