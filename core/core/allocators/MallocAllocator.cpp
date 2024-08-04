//
// Created by jglrxavpok on 02/01/2024.
//

#include <core/allocators/MallocAllocator.h>
#include <cstdlib>

#include "LowLevelMemoryAllocations.h"

namespace Carrot {
    /*static*/ MallocAllocator MallocAllocator::instance{};


    /*static*/ Allocator* Allocator::pDefault = &MallocAllocator::instance;

    /*static*/ Allocator& Allocator::getDefault() {
        return *pDefault;
    }

    /*static*/ void Allocator::setDefault(Allocator& allocator) {
        pDefault = &allocator;
    }

    MemoryBlock MallocAllocator::allocate(std::size_t size, std::size_t alignment) {
        void* ptr = Carrot::alloc(size, alignment);
        return {
            .ptr = ptr,
            .size = size
        };
    }

    void MallocAllocator::deallocate(const MemoryBlock& block) {
        Carrot::free(block.ptr);
    }

}
