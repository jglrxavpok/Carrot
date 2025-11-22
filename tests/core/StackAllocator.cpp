//
// Created by jglrxavpok on 02/01/2024.
//

#include <gtest/gtest.h>

#include <core/allocators/StackAllocator.h>

using namespace Carrot;

struct RecordingAllocator: public Allocator {
    int allocCount = 0;
    int deallocCount = 0;

    MemoryBlock allocate(std::size_t size, std::size_t alignment) override {
        allocCount++;
        return MallocAllocator::instance.allocate(size, alignment);
    }

    void deallocate(const MemoryBlock& block) override {
        deallocCount++;
        return MallocAllocator::instance.deallocate(block);
    }
};

TEST(StackAllocator, BasicAlloc) {
    StackAllocator allocator { MallocAllocator::instance };
    EXPECT_EQ(allocator.getCurrentAllocatedSize(), 0);
    MemoryBlock block = allocator.allocate(32);
    int* pInt = new (block.ptr) int;
    *pInt = 50;
    EXPECT_EQ(*pInt, 50); // attempt to access allocated memory
}

TEST(StackAllocator, DestructionDeallocatesEverything) {
    RecordingAllocator rAlloc;
    {
        StackAllocator allocator { rAlloc, 1 };
        EXPECT_EQ(allocator.getCurrentAllocatedSize(), 0);
        EXPECT_EQ(rAlloc.allocCount, 0);
        EXPECT_EQ(rAlloc.deallocCount, 0);

        allocator.allocate(1);
        allocator.allocate(1);
        allocator.allocate(1);
        allocator.allocate(1);
        allocator.allocate(1);

        EXPECT_EQ(rAlloc.allocCount, 5);
        EXPECT_EQ(rAlloc.deallocCount, 0);
    }
    EXPECT_EQ(rAlloc.allocCount, 5);
    EXPECT_EQ(rAlloc.deallocCount, 5);
}

TEST(StackAllocator, FlushAndClear) {
    RecordingAllocator rAlloc;
    StackAllocator allocator { rAlloc, 1 };
    EXPECT_EQ(allocator.getCurrentAllocatedSize(), 0);
    EXPECT_EQ(rAlloc.allocCount, 0);
    EXPECT_EQ(rAlloc.deallocCount, 0);

    allocator.allocate(1);
    allocator.allocate(1);
    allocator.allocate(1);
    allocator.allocate(1);
    allocator.allocate(1);

    EXPECT_EQ(rAlloc.allocCount, 5);
    EXPECT_EQ(rAlloc.deallocCount, 0);

    allocator.clear(); // does not free the resources, but the subsequent allocs will reuse the memory
    EXPECT_EQ(rAlloc.allocCount, 5);
    EXPECT_EQ(rAlloc.deallocCount, 0);

    allocator.allocate(1);
    allocator.allocate(1);
    allocator.allocate(1);
    allocator.allocate(1);
    allocator.allocate(1);

    EXPECT_EQ(rAlloc.allocCount, 5);
    EXPECT_EQ(rAlloc.deallocCount, 0);

    allocator.flush(); // frees the memory
    EXPECT_EQ(rAlloc.allocCount, 5);
    EXPECT_EQ(rAlloc.deallocCount, 5);
}

TEST(StackAllocator, BankSize) {
    const std::size_t bankSize = 64;
    StackAllocator allocator { MallocAllocator::instance, bankSize };
    EXPECT_EQ(allocator.getCurrentAllocatedSize(), 0);

    MemoryBlock block = allocator.allocate(32);
    EXPECT_EQ(allocator.getCurrentAllocatedSize(), bankSize); // an entire bank should have been allocated

    MemoryBlock block2 = allocator.allocate(32);
    EXPECT_EQ(allocator.getCurrentAllocatedSize(), bankSize); // no new bank should have been allocated

    MemoryBlock block3 = allocator.allocate(32);
    EXPECT_EQ(allocator.getCurrentAllocatedSize(), bankSize*2); // a new bank should have been allocated

    MemoryBlock block4 = allocator.allocate(100);
    // a new bank should have been allocated, because the requested allocation is bigger than the default bank size
    EXPECT_EQ(allocator.getCurrentAllocatedSize(), bankSize*2 + block4.size);
}

TEST(StackAllocator, ReallocateBehaviour) {
    const std::size_t bankSize = 64;
    RecordingAllocator rAlloc;
    StackAllocator allocator { rAlloc, bankSize };
    EXPECT_EQ(allocator.getCurrentAllocatedSize(), 0);
    EXPECT_EQ(rAlloc.allocCount, 0);
    EXPECT_EQ(rAlloc.deallocCount, 0);

    MemoryBlock block = allocator.allocate(32);
    EXPECT_EQ(allocator.getCurrentAllocatedSize(), bankSize); // an entire bank should have been allocated
    EXPECT_EQ(rAlloc.allocCount, 1);
    EXPECT_EQ(rAlloc.deallocCount, 0);

    MemoryBlock reallocated = allocator.reallocate(block, bankSize);
    EXPECT_EQ(reallocated.ptr, block.ptr); // reallocating a block just after allocating it: should reuse the bank if possible
    EXPECT_EQ(rAlloc.allocCount, 1); // nothing to allocate
    EXPECT_EQ(rAlloc.deallocCount, 0);

    reallocated = allocator.reallocate(reallocated, bankSize*2);
    // block was at beginning of bank, so entire back should have been deallocated then allocated again with the correct size
    EXPECT_EQ(rAlloc.allocCount, 2);
    EXPECT_EQ(rAlloc.deallocCount, 1);
    EXPECT_EQ(allocator.getCurrentAllocatedSize(), bankSize*2);
    EXPECT_EQ(allocator.getBankCount(), 1); // block was at beginning of bank, so entire back should have been deallocated then allocated again with the correct size

    EXPECT_NE(reallocated.ptr, block.ptr); // current bank is not enough for this reallocation, will be moved to a new bank
}

TEST(StackAllocator, Bug1_ClearThenReallocateWithBiggerThanInsideBanks) {
    const std::size_t bankSize = 64;
    RecordingAllocator rAlloc;
    StackAllocator allocator { rAlloc, bankSize };

    allocator.allocate(bankSize);
    allocator.allocate(bankSize*3);

    EXPECT_EQ(allocator.getBankCount(), 2);

    allocator.clear();
    EXPECT_EQ(allocator.getBankCount(), 2); // memory not modified

    allocator.allocate(bankSize*2); // first bank is too small, will get deleted, and allocation will go to previously-second-now-first bank
    EXPECT_EQ(allocator.getBankCount(), 1);
}