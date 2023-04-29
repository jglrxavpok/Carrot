//
// Created by jglrxavpok on 10/03/2021.
//

#pragma once

#include "engine/vulkan/VulkanDriver.h"
#include "BufferView.h"
#include "Buffer.h"
#include "SingleFrameStackGPUAllocator.h"

namespace Carrot {
    class ResourceAllocator {
    public:
        explicit ResourceAllocator(VulkanDriver& device);

        /**
         * Buffer used for staging uploads. Valid for the duration of an entire frame.
         */
        BufferView allocateStagingBuffer(vk::DeviceSize size);


        BufferView allocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, const std::set<uint32_t>& families = {});
        std::unique_ptr<Buffer> allocateDedicatedBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, const std::set<uint32_t>& families = {});

    public:
        /**
         * Clears ups staging resources from the previous frame
         */
        void beginFrame(const Carrot::Render::Context& renderContext);

    private:
        VulkanDriver& device;

        // TODO: smarter allocation algorithm, just making it work now
        std::vector<std::unique_ptr<Buffer>> allocatedBuffers;

        std::vector<SingleFrameStackGPUAllocator> stagingHeaps;
        std::vector<std::unique_ptr<Buffer>> dedicatedStagingBuffers;

        void freeBufferView(BufferView& view);

        friend class BufferView;
    };
}
