//
// Created by jglrxavpok on 02/01/2024.
//

#include "StackAllocator.h"

#include <core/utils/Assert.h>

namespace Carrot {
    StackAllocator::~StackAllocator() {
        flush();
    }

    MemoryBlock StackAllocator::allocate(std::size_t allocSize, std::size_t alignment) {
        auto getBank = [&]() -> MemoryBlock {
            if(!banks.empty()) {
                auto checkBank = [&](std::size_t bankIndex, std::size_t bankOffset) {
                    const MemoryBlock& bank = banks[bankIndex];
                    verify(bank.size >= bankOffset, "programming error");
                    std::size_t remainingSize = bank.size - bankOffset;
                    void* pTarget = reinterpret_cast<void*>(reinterpret_cast<std::uint64_t>(bank.ptr) + bankOffset);
                    const void* pAlignedAddress = std::align(alignment, allocSize, pTarget, remainingSize);
                    return pAlignedAddress != nullptr;
                };

                std::size_t freeBank = currentBank+1;
                if(checkBank(currentBank, cursor)) {
                    return banks[currentBank];
                } else {
                    cursor = 0; // we necessarily start from the beginning of a bank if the current bank is not enough
                    // search for a bank that can fit the allocation
                    for(; freeBank < banks.size(); freeBank++) {
                        if(checkBank(freeBank, 0)) {
                            // allocation can fit in this bank, stop search
                            break;
                        }
                    }
                }

                if(freeBank < banks.size()) {
                    currentBank = freeBank;
                    return banks[currentBank];
                }
            }

            // no free bank, allocate a new one
            const MemoryBlock bank = backingAllocator.allocate(std::max(allocSize, bankSize), alignment);
            allocatedSize += std::max(allocSize, bankSize);
            currentBank = banks.size();
            banks.emplace_back(bank);
            cursor = 0;
            return bank;
        };

        const MemoryBlock& lastBank = getBank();
        verify(lastBank.size >= cursor, "programming error: lastBank.size < cursor");
        lastAllocSize = allocSize;
        const std::size_t availableSize = lastBank.size - cursor;
        std::size_t remainingSize = availableSize;
        void* pTarget = reinterpret_cast<void*>(reinterpret_cast<std::uint64_t>(lastBank.ptr) + cursor);
        void* pAlignedAddress = std::align(alignment, allocSize, pTarget, remainingSize);
        verify(pAlignedAddress != nullptr, "bank find should have found a proper bank");
        const std::size_t alignmentCost = availableSize - remainingSize;
        cursor += alignmentCost + allocSize;
        return {
            .ptr = pAlignedAddress,
            .size = allocSize
        };
    }

    void StackAllocator::deallocate(const MemoryBlock& block) {
        if(!block.ptr) {
            return;
        }
        if(banks.empty()) {
            return;
        }

        const MemoryBlock& lastBank = banks[currentBank];
        // inside last bank
        if(reinterpret_cast<std::uint64_t>(block.ptr) >= reinterpret_cast<std::uint64_t>(lastBank.ptr)
            && reinterpret_cast<std::uint64_t>(block.ptr) + block.size <= reinterpret_cast<std::uint64_t>(lastBank.ptr) + lastBank.size) {
            // is last allocated block
            if(reinterpret_cast<std::uint64_t>(block.ptr)-reinterpret_cast<std::uint64_t>(lastBank.ptr) + block.size == cursor) {
                cursor -= block.size;
            }
        }
    }

    MemoryBlock StackAllocator::reallocate(const MemoryBlock& block, const std::size_t size, std::size_t alignment) {
        if(banks.empty()) {
            return Allocator::reallocate(block, size, alignment);
        }

        const MemoryBlock& lastBank = banks[currentBank];
        // inside last bank
        if(reinterpret_cast<std::uint64_t>(block.ptr) >= reinterpret_cast<std::uint64_t>(lastBank.ptr)
            && reinterpret_cast<std::uint64_t>(block.ptr) + block.size <= reinterpret_cast<std::uint64_t>(lastBank.ptr) + lastBank.size) {
            // is last allocated block
            if(reinterpret_cast<std::uint64_t>(block.ptr)-reinterpret_cast<std::uint64_t>(lastBank.ptr) + block.size == cursor) {
                std::size_t allocationStart = cursor - block.size;
                if(size + allocationStart > lastBank.size) {
                    return Allocator::reallocate(block, size, alignment); // does not fit in current bank
                }
                return MemoryBlock {
                    .ptr = block.ptr,
                    .size = size,
                }; // fits in current bank and at current location. No need for any copy
            }
        }
        return Allocator::reallocate(block, size, alignment);
    }

    void StackAllocator::clear() {
        cursor = 0;
        currentBank = 0;
    }

    void StackAllocator::flush() {
        clear();
        for(auto it = banks.cbegin(); it != banks.cend(); ++it) {
            backingAllocator.deallocate(*it);
        }
        banks.clear();
        allocatedSize = 0;
    }

    std::size_t StackAllocator::getCurrentAllocatedSize() const {
        return allocatedSize;
    }

} // Carrot