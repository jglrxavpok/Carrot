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
#include <core/io/Logging.hpp> // for debug

Carrot::Async::ParallelMap<vk::DeviceAddress, const Carrot::Buffer*> Carrot::Buffer::BufferByStartAddress;

Carrot::Buffer::Buffer(VulkanDriver& driver, vk::DeviceSize size, vk::BufferUsageFlags usage,
                       vk::MemoryPropertyFlags properties, std::set<uint32_t> families): driver(driver), size(size) {
    ZoneScopedN("Buffer creation");
    deviceLocal = (properties & vk::MemoryPropertyFlagBits::eDeviceLocal) == vk::MemoryPropertyFlagBits::eDeviceLocal;

    usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;

    auto& queueFamilies = driver.getQueuePartitioning();
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

    memory = Carrot::DeviceMemory(allocInfo.get());
    driver.getLogicalDevice().bindBufferMemory(*vkBuffer, memory.getVulkanMemory(), 0);

    deviceAddress = driver.getLogicalDevice().getBufferAddress({.buffer = *vkBuffer});
    BufferByStartAddress.getOrCompute(deviceAddress, [&]() {
       return this;
    });
}

void Carrot::Buffer::asyncCopyTo(vk::Semaphore& semaphore, Carrot::Buffer& other, vk::DeviceSize srcOffset, vk::DeviceSize dstOffset) const {
    getWholeView().subView(srcOffset).copyTo(semaphore, other.getWholeView().subView(dstOffset));
}

void Carrot::Buffer::copyTo(Carrot::Buffer& other, vk::DeviceSize srcOffset, vk::DeviceSize dstOffset) const {
    getWholeView().subView(srcOffset).copyToAndWait(other.getWholeView().subView(dstOffset));
}

void Carrot::Buffer::copyTo(std::span<std::uint8_t> out, vk::DeviceSize offset) const {
    // allocate staging buffer used for transfer
    auto stagingBuffer = Carrot::Buffer(driver, out.size(), vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, std::set<uint32_t>{driver.getQueuePartitioning().transferFamily.value()});

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

    stagingBuffer.destroyNow();
}

void Carrot::Buffer::directDownload(std::span<std::uint8_t> out, vk::DeviceSize offset) {
    void* pData;
    if(driver.getLogicalDevice().mapMemory(memory.getVulkanMemory(), offset, out.size(), static_cast<vk::MemoryMapFlags>(0), &pData) != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to map memory!");
    }
    std::memcpy(out.data(), reinterpret_cast<const uint8_t*>(pData), out.size());
    driver.getLogicalDevice().unmapMemory(memory.getVulkanMemory());
}

const vk::Buffer& Carrot::Buffer::getVulkanBuffer() const {
    return *vkBuffer;
}

void Carrot::Buffer::directUpload(const void* data, vk::DeviceSize length, vk::DeviceSize offset) {
    void* pData = map<void>();
    std::memcpy(static_cast<uint8_t*>(pData) + offset, data, length);
}

uint64_t Carrot::Buffer::getSize() const {
    return size;
}

Carrot::Buffer::~Buffer() {
    if(mappedPtr) {
        unmap();
    }
    GetVulkanDriver().deferDestroy(debugName, std::move(vkBuffer));
    GetVulkanDriver().deferDestroy(debugName+"-memory", std::move(memory));

    BufferByStartAddress.remove(deviceAddress);
    deviceAddress = 0x0;
}

void Carrot::Buffer::setDebugNames(const std::string& name) {
    debugName = name;
    nameSingle(name, getVulkanBuffer());
}

const std::string& Carrot::Buffer::getDebugName() const {
    return debugName;
}

void* Carrot::Buffer::mapGeneric() {
    if(mappedPtr) {
        return mappedPtr;
    }
    void* ptr = driver.getLogicalDevice().mapMemory(memory.getVulkanMemory(), 0, VK_WHOLE_SIZE);
    mappedPtr = ptr;
    return ptr;
}


void Carrot::Buffer::unmap() {
    verify(mappedPtr != nullptr, "Must be mapped!");
    driver.getLogicalDevice().unmapMemory(memory.getVulkanMemory());
    mappedPtr = nullptr;
}

vk::DeviceAddress Carrot::Buffer::getDeviceAddress() const {
    return deviceAddress;
}

vk::DescriptorBufferInfo Carrot::Buffer::asBufferInfo() const {
    return vk::DescriptorBufferInfo {
        .buffer = getVulkanBuffer(),
        .offset = 0,
        .range = size,
    };
}

Carrot::BufferView Carrot::Buffer::getWholeView() {
    return BufferView(nullptr /* allocator does NOT own this buffer view */, *this, 0u, static_cast<vk::DeviceSize>(size));
}

const Carrot::BufferView Carrot::Buffer::getWholeView() const {
    return BufferView(nullptr /* allocator does NOT own this buffer view */, *const_cast<Carrot::Buffer*>(this), 0u, static_cast<vk::DeviceSize>(size));
}

void Carrot::Buffer::flushMappedMemory(vk::DeviceSize start, vk::DeviceSize length) {
    driver.getLogicalDevice().flushMappedMemoryRanges(vk::MappedMemoryRange {
       .memory = memory.getVulkanMemory(),
       .offset = start,
       .size = length,
    });
}

void Carrot::Buffer::invalidateMappedRange(vk::DeviceSize start, vk::DeviceSize length) {
    driver.getLogicalDevice().invalidateMappedMemoryRanges(vk::MappedMemoryRange {
       .memory = memory.getVulkanMemory(),
       .offset = start,
       .size = length,
    });
}

void Carrot::Buffer::destroyNow() {
    if(mappedPtr) {
        unmap();
    }
    vkBuffer.reset();
    memory = {};
}

Carrot::BufferAllocation Carrot::Buffer::internalStagingBuffer(vk::DeviceSize size) {
    return GetResourceAllocator().allocateStagingBuffer(size);
}

void DebugDumpAllBuffers() {
    Carrot::Log::info("=== DebugDumpAllBuffers ===");
    for(const auto& [key, ppBuffer] : Carrot::Buffer::BufferByStartAddress.snapshot()) {
        if(ppBuffer && *ppBuffer) {
            const Carrot::Buffer& buffer = *(*ppBuffer);
            Carrot::Log::info("- Name '%s', device local '%d', size '%llu'",
                              buffer.getDebugName().c_str(),
                              buffer.isDeviceLocal(),
                              (std::uint64_t)buffer.getSize());
        }
    }
    Carrot::Log::info("============================");
    Carrot::Log::flush();
}