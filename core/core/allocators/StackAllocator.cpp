//
// Created by jglrxavpok on 02/01/2024.
//

#include "StackAllocator.h"

#include <core/utils/Assert.h>

#include <ranges>

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
                    if(cursor != 0) { // if not at start of current bank, don't modify it (because someone is using the memory)
                        currentBank++;
                        cursor = 0;
                    }

                    // remove all banks starting from current bank that cannot fit allocation
                    while (currentBank < banks.size() && !checkBank(currentBank, 0)) {
                        deallocateBank(currentBank);
                    }
                }

                // if found available bank, use it
                if(currentBank < banks.size()) {
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
        verify(!banks.empty() || block.ptr == nullptr, "This allocator is empty, the block is not from this allocator or was already freed!");
        if(banks.empty() || block.ptr == nullptr) {
            return allocate(size, alignment);
        }

        const MemoryBlock& lastBank = banks[currentBank];
        // inside last bank
        if(reinterpret_cast<std::uint64_t>(block.ptr) >= reinterpret_cast<std::uint64_t>(lastBank.ptr)
            && reinterpret_cast<std::uint64_t>(block.ptr) + block.size <= reinterpret_cast<std::uint64_t>(lastBank.ptr) + lastBank.size) {
            // is last allocated block
            if(reinterpret_cast<std::uint64_t>(block.ptr)-reinterpret_cast<std::uint64_t>(lastBank.ptr) + block.size == cursor) {
                const std::size_t allocationStart = cursor - block.size;

                const bool deallocateCurrentBank = cursor - block.size == 0; // at beginning of current bank

                // todo: does not take alignment into account
                if(size + allocationStart > lastBank.size) {
                    const std::size_t currentBankBeforeRealloc = currentBank;
                    MemoryBlock reallocResult = Allocator::reallocate(block, size, alignment); // does not fit in current bank
                    if(deallocateCurrentBank) {
                        allocatedSize -= banks[currentBankBeforeRealloc].size;
                        backingAllocator.deallocate(banks[currentBankBeforeRealloc]);
                        banks.erase(banks.begin() + currentBankBeforeRealloc);
                        currentBank--;
                    }
                    return reallocResult;
                }
                cursor -= block.size;
                cursor += size;
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
        for(auto& bank : std::ranges::reverse_view(banks)) {
            backingAllocator.deallocate(bank);
        }
        banks.clear();
        allocatedSize = 0;
    }

    std::size_t StackAllocator::getCurrentAllocatedSize() const {
        return allocatedSize;
    }

    std::size_t StackAllocator::getBankCount() const {
        return banks.size();
    }

    void StackAllocator::deallocateBank(std::size_t bankIndex) {
        verify(bankIndex >= currentBank, "Cannot deallocate banks before current bank, they still have used data!");
        if(bankIndex == currentBank) {
            verify(cursor == 0, "Cannot deallocate current bank while there is still so data in it!");
        }

        allocatedSize -= banks[bankIndex].size;
        backingAllocator.deallocate(banks[bankIndex]);
        banks.erase(banks.begin() + bankIndex);
    }

} // Carrot