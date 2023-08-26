//
// Created by jglrxavpok on 10/03/2021.
//

#include "ResourceAllocator.h"
#include <engine/Engine.h>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>

static vk::DeviceSize HeapSize = 1024 * 1024 * 1024; // 1Gib, no particular reason for this exact value

namespace Carrot {
    ResourceAllocator::ResourceAllocator(VulkanDriver& device): device(device) {
        heap = std::make_unique<Buffer>(device, HeapSize,
                                        vk::BufferUsageFlagBits::eTransferSrc,
                                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                        std::set<uint32_t>{GetVulkanDriver().getQueueFamilies().transferFamily.value()});
        heap->setDebugNames("ResourceAllocator heap");
        VmaVirtualBlock block;
        VmaVirtualBlockCreateInfo blockInfo = {};
        blockInfo.size = HeapSize;
        VkResult r = vmaCreateVirtualBlock(&blockInfo, &block);

        verify(r == VK_SUCCESS, Carrot::sprintf("Failed to create virtual block (%s)", string_VkResult(r)));
        verify(block != nullptr, "Failed to create virtual block");
        virtualBlock = block;
    }

    ResourceAllocator::~ResourceAllocator() {
        vmaDestroyVirtualBlock((VmaVirtualBlock)virtualBlock);
    }

    StagingBuffer ResourceAllocator::allocateStagingBuffer(vk::DeviceSize size) {
        auto makeDedicated = [&]() {
            Carrot::Async::LockGuard g { dedicatedBuffersAccess };
            StagingBuffer result { this };
            dedicatedStagingBuffers.emplace_back(allocateDedicatedBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, std::set<uint32_t>{GetVulkanDriver().getQueueFamilies().transferFamily.value()}));
            auto& pBuffer = dedicatedStagingBuffers.back();
            result.allocation = pBuffer.get();
            result.dedicated = true;
            result.view = pBuffer->getWholeView();
            return result;
        };

        if(size > HeapSize) {
            return makeDedicated();
        }

        VmaVirtualBlock block = reinterpret_cast<VmaVirtualBlock>(virtualBlock);
        VmaVirtualAllocationCreateInfo allocInfo = {};
        allocInfo.size = size;

        VmaVirtualAllocation alloc;
        VkDeviceSize offset;
        VkResult result = vmaVirtualAllocate(block, &allocInfo, &alloc, &offset);
        if(result == VK_SUCCESS) {
            StagingBuffer resultBuffer { this };
            resultBuffer.allocation = alloc;
            resultBuffer.dedicated = false;
            resultBuffer.view = heap->getWholeView().subView(offset, size);
            return resultBuffer;
        } else {
            // if we reach here, no block has space available, create a dedicated buffer
            return makeDedicated();
        }
    }

    void ResourceAllocator::beginFrame(const Render::Context& renderContext) {
    }

    BufferView ResourceAllocator::allocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                                                 vk::MemoryPropertyFlags properties,
                                                                 const std::set<uint32_t>& families) {
        // TODO: proper allocation algorithm
        auto buffer = allocateDedicatedBuffer(size, usage, properties, families);
        auto result = buffer->getWholeView();
        allocatedBuffers.emplace_back(move(buffer));
        return result;
    }

    std::unique_ptr<Buffer> ResourceAllocator::allocateDedicatedBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                                                          vk::MemoryPropertyFlags properties,
                                                                          const std::set<uint32_t>& families) {
        return std::make_unique<Buffer>(device, size, usage, properties, families);
    }

    void ResourceAllocator::freeBufferView(BufferView& view) {
        // TODO: proper allocation algorithm
        std::erase_if(allocatedBuffers, [&](const auto& p) { return p->getVulkanBuffer() == view.getBuffer().getVulkanBuffer(); });
    }

    void ResourceAllocator::freeStagingBuffer(StagingBuffer* buffer) {
        if(buffer->dedicated) {
            Carrot::Async::LockGuard g { dedicatedBuffersAccess };
            std::erase_if(dedicatedStagingBuffers, [&](const std::unique_ptr<Carrot::Buffer>& pBuffer) {
                return pBuffer.get() == (Carrot::Buffer*)buffer->allocation;
            });
        } else {
            vmaVirtualFree((VmaVirtualBlock)virtualBlock, (VmaVirtualAllocation)buffer->allocation);
        }
    }
}