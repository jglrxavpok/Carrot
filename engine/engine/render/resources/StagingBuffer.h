//
// Created by jglrxavpok on 25/08/2023.
//

#pragma once

#include "BufferView.h"

namespace Carrot {
    class ResourceAllocator;

    struct StagingBuffer {
        BufferView view;

        ~StagingBuffer();

    private:
        explicit StagingBuffer(ResourceAllocator* allocator);

        StagingBuffer(const StagingBuffer& other);
        StagingBuffer(StagingBuffer&& other) noexcept;

        StagingBuffer& operator=(const StagingBuffer&);
        StagingBuffer& operator=(StagingBuffer&&) noexcept;

        void free();

        ResourceAllocator* allocator = nullptr;

        void* allocation = nullptr; // Carrot::Buffer* if dedicated, VmaVirtualAllocation otherwise
        bool dedicated = false;

        friend class ResourceAllocator;
    };
}