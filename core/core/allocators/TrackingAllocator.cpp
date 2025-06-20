//
// Created by jglrxavpok on 20/06/25.
//

#include "TrackingAllocator.h"

namespace Carrot {
    TrackingAllocator::TrackingAllocator(Allocator& wrapped): wrapped(wrapped) {}

    MemoryBlock TrackingAllocator::allocate(std::size_t size, std::size_t alignment) {
        MemoryBlock block = wrapped.allocate(size, alignment);
        if (block.ptr) {
            ++allocationCount;
            ++liveAllocationCount;
            allocationAmount += size;
            liveAllocationAmount += size;
        }
        return block;
    }

    void TrackingAllocator::deallocate(const MemoryBlock& block) {
        if (block.ptr) {
            liveAllocationAmount -= block.size;
            --liveAllocationCount;
        }
        wrapped.deallocate(block);
    }

    bool TrackingAllocator::isCompatibleWith(const Allocator& other) const {
        return &other == &wrapped;
    }

}
