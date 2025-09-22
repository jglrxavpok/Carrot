//
// Created by jglrxavpok on 10/03/2021.
//

#pragma once

#include <vk_mem_alloc.h>
#include <core/UniquePtr.hpp>
#include <core/containers/Pair.hpp>
#include <core/containers/Vector.hpp>

#include "BufferView.h"
#include "Buffer.h"
#include "SingleFrameStackGPUAllocator.h"
#include "BufferAllocation.h"

struct VmaVirtualAllocationCreateInfo;

namespace Carrot {
    namespace Render {
        struct Context;
    }

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
         * @param stride must not be 0, this is the distance in bytes between the start of two output BufferAllocation, and the start of two 'size' values
         * @param outputs where to store the allocation results
         * @param sizes sizes of the buffers to allocate
         * @param usageFlags usage flag for all buffers allocated
         */
        void multiAllocateDeviceBuffer(std::size_t count, std::size_t stride, BufferAllocation* outputs, const vk::DeviceSize* sizes, vk::BufferUsageFlags usageFlags);

        /**
         * Allocates multi device buffer used for device-local storage
         * This function locks the allocator only once, so this is faster than multiple single allocations
         * @param output where to store the allocation results
         * @param sizes sizes of the buffers to allocate
         * @param usageFlags usage flag for all buffers allocated
         */
        void multiAllocateDeviceBuffer(std::span<BufferAllocation> output, std::span<const vk::DeviceSize> sizes, vk::BufferUsageFlags usageFlags);


        BufferView allocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, const std::set<uint32_t>& families = {});
        UniquePtr<Buffer> allocateDedicatedBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, const std::set<uint32_t>& families = {});

    public:
        /**
         * Clears ups staging resources from the previous frame
         */
        void beginFrame(const Carrot::Render::Context& renderContext);

    private:
        vk::DeviceSize computeAlignment(vk::BufferUsageFlags usageFlags) const;

        void freeStagingBuffer(BufferAllocation* buffer);

        BufferAllocation allocateInHeap(const VmaVirtualAllocationCreateInfo& allocInfo,
                                        void* virtualBlock,
                                        Carrot::Buffer& heapStorage,
                                        std::function<BufferAllocation(vk::DeviceSize)> makeDedicated);

        VulkanDriver& device;

        // TODO: smarter allocation algorithm, just making it work now
        Vector<UniquePtr<Buffer>> allocatedBuffers;

        Carrot::Async::SpinLock stagingAccess;
        Carrot::Async::SpinLock deviceAccess;
        Carrot::Async::SpinLock graveyardAccess;
        Vector<UniquePtr<Buffer>> dedicatedStagingBuffers;
        Vector<UniquePtr<Buffer>> dedicatedDeviceBuffers;

        Vector<Pair<std::uint32_t, UniquePtr<Buffer>>> dedicatedBufferGraveyard;
        Vector<Triplet<std::uint32_t, VmaVirtualAllocation, bool/* is staging buffer */>> nonDedicatedBufferGraveyard;

        std::uint32_t currentFrame = 0;

        vk::PhysicalDeviceLimits physicalLimits;
        vk::PhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties;

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
