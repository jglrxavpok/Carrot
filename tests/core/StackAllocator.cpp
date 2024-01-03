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


