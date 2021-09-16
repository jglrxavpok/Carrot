//
// Created by jglrxavpok on 10/03/2021.
//

#pragma once

#include "engine/vulkan/includes.h"
#include "Buffer.h"
#include <memory>

namespace Carrot {
    class ResourceAllocator;

    class BufferView {
    private:
        Carrot::ResourceAllocator* allocator = nullptr;
        Carrot::Buffer& buffer;
        vk::DeviceSize start;
        vk::DeviceSize size;

    public:
        explicit BufferView(Carrot::ResourceAllocator* allocator, Carrot::Buffer& buffer, vk::DeviceSize start, vk::DeviceSize size);

        [[nodiscard]] vk::DeviceSize getStart() const { return start; };
        [[nodiscard]] vk::DeviceSize getSize() const { return size; };

        [[nodiscard]] Buffer& getBuffer() { return buffer; };
        [[nodiscard]] const Buffer& getBuffer() const { return buffer; };
        [[nodiscard]] const vk::Buffer& getVulkanBuffer() const { return getBuffer().getVulkanBuffer(); };

        [[nodiscard]] vk::DescriptorBufferInfo asBufferInfo() const;

        void flushMappedMemory();

        template<typename T>
        T* map();

        void unmap();

        ~BufferView();
    };

}

#include "BufferView.ipp"
