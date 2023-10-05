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
        BufferAllocation allocateStagingBuffer(vk::DeviceSize size);

        /**
         * Buffer used for device-local storage
         */
        BufferAllocation allocateDeviceBuffer(vk::DeviceSize size, vk::BufferUsageFlags usageFlags);


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
                                        std::function<BufferAllocation()> makeDedicated);

        VulkanDriver& device;

        // TODO: smarter allocation algorithm, just making it work now
        std::vector<std::unique_ptr<Buffer>> allocatedBuffers;

        Carrot::Async::SpinLock dedicatedStagingBuffersAccess;
        Carrot::Async::SpinLock dedicatedDeviceBuffersAccess;
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
