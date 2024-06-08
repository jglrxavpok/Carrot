//
// Created by jglrxavpok on 02/01/2024.
//

#include "InlineAllocator.h"
#include <core/utils/Assert.h>

namespace Carrot {
    template<std::size_t MaxSize>
    MemoryBlock InlineAllocator<MaxSize>::allocate(std::size_t size, std::size_t alignment) {
        verify(reinterpret_cast<std::size_t>(pData) % alignment == 0, "Trying to allocate a block with the wrong alignment! Align this allocator properly.");
        if(alreadyAllocated || size > MaxSize) {
            throw OutOfMemoryException();
        }
        alreadyAllocated = true;
        return {
            .ptr = pData,
            .size = MaxSize,
        };
    }

    template<std::size_t MaxSize>
    void InlineAllocator<MaxSize>::deallocate(const MemoryBlock& block) {
        verify(alreadyAllocated, "Cannot deallocate if nothing was allocated");
        verify(block.ptr == pData, "The provided block does not belong to this allocator!");
        alreadyAllocated = false;
    }
} // Carrot