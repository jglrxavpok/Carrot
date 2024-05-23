//
// Created by jglrxavpok on 25/08/2023.
//

#pragma once

#include "BufferView.h"
#include <engine/vulkan/DebugNameable.h>

namespace Carrot {
    class ResourceAllocator;

    struct BufferAllocation: public DebugNameable {
        struct DebugData {
            std::string name;
            vk::DeviceSize size;
            bool isDedicated;
        };
        static Carrot::Async::ParallelMap<vk::DeviceAddress, DebugData> AllocationsByStartAddress;

        BufferView view;

        BufferAllocation() = default;
        ~BufferAllocation();

        BufferAllocation(BufferAllocation&& other) noexcept;
        BufferAllocation& operator=(BufferAllocation&&) noexcept;

    protected:
        void setDebugNames(const std::string& name) override final;

    private:
        explicit BufferAllocation(ResourceAllocator* allocator);

        BufferAllocation(const BufferAllocation& other);

        BufferAllocation& operator=(const BufferAllocation&);

        void free();

        ResourceAllocator* allocator = nullptr;

        void* allocation = nullptr; // Carrot::Buffer* if dedicated, VmaVirtualAllocation otherwise
        bool dedicated = false;
        bool staging = false;

        friend class ResourceAllocator;
    };
}