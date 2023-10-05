//
// Created by jglrxavpok on 25/08/2023.
//

#include "BufferAllocation.h"
#include "ResourceAllocator.h"

namespace Carrot {
    BufferAllocation::BufferAllocation(ResourceAllocator *allocator): allocator(allocator) {}

    BufferAllocation::BufferAllocation(const BufferAllocation& toCopy) {
        *this = toCopy;
    }

    BufferAllocation::BufferAllocation(BufferAllocation&& toMove) noexcept {
        *this = std::move(toMove);
    }

    BufferAllocation& BufferAllocation::operator=(const BufferAllocation& o) {
        if(this == &o) {
            return *this;
        }
        free();
        allocator = o.allocator;
        allocation = o.allocation;
        view = o.view;
        staging = o.staging;
        dedicated = o.dedicated;
        return *this;
    }

    BufferAllocation& BufferAllocation::operator=(BufferAllocation&& o) noexcept {
        if(this == &o) {
            return *this;
        }
        free();
        allocation = o.allocation;
        o.allocation = nullptr;

        allocator = o.allocator;
        o.allocator = nullptr;
        view = o.view;
        o.view = BufferView{};

        staging = o.staging;
        dedicated = o.dedicated;
        return *this;
    }

    void BufferAllocation::free() {
        if(!allocator) {
            return; // was moved
        }
        allocator->freeStagingBuffer(this);
    }

    BufferAllocation::~BufferAllocation() {
        free();
    }
}