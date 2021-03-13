//
// Created by jglrxavpok on 10/03/2021.
//

#pragma once

#include <engine/vulkan/includes.h>
#include "engine/vulkan/VulkanDriver.h"
#include "BufferView.h"
#include "Buffer.h"

namespace Carrot {
    class ResourceAllocator {
    private:
        VulkanDriver& device;

        // TODO: smarter allocation algorithm, just making it work now
        vector<unique_ptr<Buffer>> allocatedBuffers;

        void freeBufferView(BufferView& view);

        friend class BufferView;

    public:
        explicit ResourceAllocator(VulkanDriver& device);

        BufferView allocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, const std::set<uint32_t>& families = {});
        unique_ptr<Buffer> allocateDedicatedBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, const std::set<uint32_t>& families = {});
    };
}
