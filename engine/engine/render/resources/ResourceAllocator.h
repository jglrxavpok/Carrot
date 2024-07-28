//
// Created by jglrxavpok on 10/03/2021.
//

#pragma once

#include "engine/vulkan/VulkanDriver.h"
#include "BufferView.h"
#include "Buffer.h"
#include "SingleFrameStackGPUAllocator.h"
#include "BufferAllocation.h"

struct VmaVirtualAllocationCreateInfo;

namespace Carrot {
    class ResourceAllocator {
    public:
        explicit ResourceAllocator(VulkanDriver& device);
        ~ResourceAllocator();

        /**
         * Buffer used for staging uploads
         */
        BufferAllocation allocateStagingBuffer(vk::DeviceSize size, vk::DeviceSize alignment = 1);

        /**
         * Buffer used for device-local storage
         */
        BufferAllocation allocateDeviceBuffer(vk::DeviceSize size, vk::BufferUsageFlags usageFlags);

        /**
         * Allocates multi device buffer used for device-local storage
         * This function locks the allocator only once, so this is faster than multiple single allocations
         * @param output where to store the allocation results
         * @param sizes sizes of the buffers to allocate
         * @param usageFlags usage flag for all buffers allocated
         */
        void multiAllocateDeviceBuffer(std::span<BufferAllocation> output, std::span<const vk::DeviceSize> sizes, vk::BufferUsageFlags usageFlags);


        BufferView allocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, const std::set<uint32_t>& families = {});
        std::unique_ptr<Buffer> allocateDedicatedBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, const std::set<uint32_t>& families = {});

    public:
        /**
         * Clears ups staging resources from the previous frame
         */
        void beginFrame(const Carrot::Render::Context& renderContext);

    private:
        void freeStagingBuffer(BufferAllocation* buffer);

        BufferAllocation allocateInHeap(const VmaVirtualAllocationCreateInfo& allocInfo,
                                        void* virtualBlock,
                                        Carrot::Buffer& heapStorage,
                                        std::function<BufferAllocation(vk::DeviceSize)> makeDedicated);

        VulkanDriver& device;

        // TODO: smarter allocation algorithm, just making it work now
        std::vector<std::unique_ptr<Buffer>> allocatedBuffers;

        Carrot::Async::SpinLock stagingAccess;
        Carrot::Async::SpinLock deviceAccess;
        std::vector<std::unique_ptr<Buffer>> dedicatedStagingBuffers;
        std::vector<std::unique_ptr<Buffer>> dedicatedDeviceBuffers;

        std::unique_ptr<Carrot::Buffer> stagingHeap;
        std::unique_ptr<Carrot::Buffer> deviceHeap;
        // VmaVirtualBlock
        void* stagingVirtualBlock = nullptr;
        void* deviceVirtualBlock = nullptr;

        void freeBufferView(BufferView& view);

        friend class BufferView;
        friend class BufferAllocation;
    };
}
