//
// Created by jglrxavpok on 20/06/25.
//

#pragma once
#include <atomic>
#include <core/Allocator.h>
#include <core/utils/Types.h>

namespace Carrot {
    /**
     * Allocator that tracks how much memory is currently used (and relays calls to another allocator)
     */
    struct TrackingAllocator: Allocator {
        /// Total count of calls to allocate
        std::atomic<i64> allocationCount = 0;
        /// Total count of allocated memory through this allocator
        std::atomic<i64> allocationAmount = 0;

        /// Count of allocations made through this allocator that are not freed yet
        std::atomic<i64> liveAllocationCount = 0;

        /// Count of allocated bytes that are not freed yet
        std::atomic<i64> liveAllocationAmount = 0;

        TrackingAllocator(Allocator& wrapped);

        MemoryBlock allocate(std::size_t size, std::size_t alignment) override;
        void deallocate(const MemoryBlock& block) override;
        bool isCompatibleWith(const Allocator& other) const override;

    private:
        Allocator& wrapped;
    };
}
