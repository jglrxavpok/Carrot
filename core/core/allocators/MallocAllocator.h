//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

#include <core/Allocator.h>

namespace Carrot {
    struct MallocAllocator: public Allocator {
        static MallocAllocator instance;

        MemoryBlock allocate(std::size_t size, std::size_t alignment = 1) override;

        void deallocate(const MemoryBlock& block) override;
    };
}