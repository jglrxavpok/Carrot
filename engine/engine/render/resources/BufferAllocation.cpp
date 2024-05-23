//
// Created by jglrxavpok on 25/08/2023.
//

#include "BufferAllocation.h"
#include "ResourceAllocator.h"

namespace Carrot {
    /* static */ Carrot::Async::ParallelMap<vk::DeviceAddress, BufferAllocation::DebugData> BufferAllocation::AllocationsByStartAddress;

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

    void BufferAllocation::setDebugNames(const std::string& name) {
        if(!view) {
            return; // was moved
        }
        vk::DeviceAddress addr = view.getDeviceAddress();
        AllocationsByStartAddress.remove(addr);
        AllocationsByStartAddress.getOrCompute(addr, [&]() {
            return DebugData {
                .name = name,
                .size = view.getSize(),
                .isDedicated = dedicated,
            };
        });
    }

    void BufferAllocation::free() {
        if(!allocator) {
            return; // was moved
        }
        vk::DeviceAddress addr = view.getDeviceAddress();
        AllocationsByStartAddress.remove(addr);
        allocator->freeStagingBuffer(this);
    }

    BufferAllocation::~BufferAllocation() {
        free();
    }
}