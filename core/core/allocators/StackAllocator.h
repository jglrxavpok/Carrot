//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

#include <vector>
#include <core/Allocator.h>
#include <core/allocators/MallocAllocator.h>

namespace Carrot {
    /**
     * \brief Allocator which allocates blocks of memory contiguously from memory blocks returned by the backing allocator.
     * Deallocating a block which is not the latest-allocated one will fragment the memory.
     *
     * Unless the requested size of an allocation is greater than the bank size, this allocator attempts to batch allocations into
     * contiguous blocks of memory (banks). The size of banks is controllable via the constructor.
     */
    class StackAllocator: public Allocator {
    public:
        /**
         * \brief Creates a new StackAllocator with the given allocator as a backing allocator.
         * \param allocator backing allocator to use for allocating blocks
         * \param defaultBankSize
         */
        explicit StackAllocator(Allocator& allocator, std::size_t defaultBankSize = 1024): backingAllocator(allocator), bankSize(defaultBankSize) {}
        ~StackAllocator();
        
        MemoryBlock allocate(std::size_t size, std::size_t alignment = 1) override;

        void deallocate(const MemoryBlock& block) override;

        MemoryBlock reallocate(const MemoryBlock& block, const std::size_t size, std::size_t alignment = 1) override;

        /**
         * How many bytes have been allocated (in total) by this allocator. This takes into account the overhead/wasted space due to bank reallocations
         */
        std::size_t getCurrentAllocatedSize() const;

        std::size_t getBankCount() const;

        /**
         * \brief Deallocate all memory held by this allocator
         */
        void flush();

        /**
         * \brief Resets the state of this allocator as-if no allocation were made, BUT keeps the allocated memory.
         * Use this when you want to reuse already allocated memory.
         */
        void clear();

    private:
        void deallocateBank(std::size_t bankIndex);

        Allocator& backingAllocator;
        std::size_t bankSize = 0;

        // TODO: replace with std::vector replacement
        std::vector<MemoryBlock> banks;
        std::size_t currentBank = 0;
        std::size_t cursor = 0; // cursor inside current bank
        std::size_t allocatedSize = 0;

     std::size_t lastAllocSize = 0;
    };

} // Carrot
