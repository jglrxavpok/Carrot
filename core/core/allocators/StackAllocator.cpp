//
// Created by jglrxavpok on 02/01/2024.
//

#include "StackAllocator.h"

namespace Carrot {
    StackAllocator::~StackAllocator() {
        flush();
    }

    MemoryBlock StackAllocator::allocate(std::size_t size, std::size_t alignment) {
        auto allocBank = [&](std::size_t bankSize, std::size_t alignment) -> MemoryBlock {
            std::size_t freeBank = currentBank+1;
            // search for a bank that can fit the allocation
            for(; freeBank < banks.size(); freeBank++) {
                const MemoryBlock& bank = banks[freeBank];
                const std::size_t remainingSize = bank.size - cursor;
                std::size_t bankSize = remainingSize;
                void* pTarget = reinterpret_cast<void*>(reinterpret_cast<std::uint64_t>(bank.ptr) + cursor);
                const void* pAlignedAddress = std::align(alignment, size, pTarget, bankSize);
                if(pAlignedAddress != nullptr) { // allocation can fit in this bank, stop search
                    break;
                }
            }

            if(freeBank < banks.size()) {
                currentBank = freeBank;
                return banks[currentBank];
            } else {
                // no free bank, allocate a new one
                const MemoryBlock bank = backingAllocator.allocate(bankSize, alignment);
                allocatedSize += bankSize;
                currentBank++;
                banks.emplace_back(bank);
                return bank;
            }
        };

        if(banks.empty()) {
            // first allocation
            const MemoryBlock bank = allocBank(std::max(size, bankSize), alignment);
            currentBank = 0;
            cursor = size;
            return {
                .ptr = bank.ptr,
                .size = size,
            };
        } else {
            const MemoryBlock& lastBank = banks[currentBank];
            std::size_t remainingSize = lastBank.size - cursor;
            void* pTarget = reinterpret_cast<void*>(reinterpret_cast<std::uint64_t>(lastBank.ptr) + cursor);
            void* pAlignedAddress = std::align(alignment, size, pTarget, remainingSize);
            if(pAlignedAddress == nullptr) { // could not fit inside current bank, need a new one
                const MemoryBlock bank = allocBank(std::max(size, bankSize), alignment);
                cursor = size;
                return {
                    .ptr = bank.ptr,
                    .size = size,
                };
            } else {
                cursor += lastBank.size - remainingSize;
                return {
                    .ptr = pAlignedAddress,
                    .size = size
                };
            }
        }
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