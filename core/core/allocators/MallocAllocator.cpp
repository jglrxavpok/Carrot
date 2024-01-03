//
// Created by jglrxavpok on 02/01/2024.
//

#include <core/allocators/MallocAllocator.h>
#include <cstdlib>

namespace Carrot {
    /*static*/ MallocAllocator MallocAllocator::instance{};

    MemoryBlock MallocAllocator::allocate(std::size_t size, std::size_t alignment) {
#ifdef WIN32
        void* ptr = _aligned_malloc(size, alignment);
#else
        void* ptr = std::aligned_alloc(size, alignment);
#endif
        return {
            .ptr = ptr,
            .size = size
        };
    }

    void MallocAllocator::deallocate(const MemoryBlock& block) {
#ifdef WIN32
        _aligned_free(block.ptr);
#else
        free(block.ptr);
#endif
    }

}
