//
// Created by jglrxavpok on 02/01/2024.
//

#include <core/Macros.h>
#include <gtest/gtest.h>
#include <core/allocators/InlineAllocator.h>

using namespace Carrot;

TEST(InlineAllocator, BasicAlloc) {
    InlineAllocator<64> alloc;
    MemoryBlock block = alloc.allocate(32);

    std::uint32_t* ptr = new (block.ptr) std::uint32_t;
    *ptr = 50;
    EXPECT_EQ(block.size, decltype(alloc)::Size); // entire allocator is used
    EXPECT_EQ(*ptr, 50); // attempt access to allocated memory
}

TEST(InlineAllocator, OnlyOnce) {
    InlineAllocator<64> alloc;
    DISCARD(alloc.allocate(32));

    // this allocator accepts a single alive allocation at once
    EXPECT_THROW(alloc.allocate(32), OutOfMemoryException);
}

TEST(InlineAllocator, AllocDeallocThenRealloc) {
    InlineAllocator<64> alloc;
    MemoryBlock block = alloc.allocate(32);
    alloc.deallocate(block);

    MemoryBlock block2 = alloc.allocate(32);
    EXPECT_EQ(block.ptr, block2.ptr); // some block should be allocated
}