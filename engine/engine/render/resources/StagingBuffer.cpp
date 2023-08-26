//
// Created by jglrxavpok on 25/08/2023.
//

#include "StagingBuffer.h"
#include "ResourceAllocator.h"

namespace Carrot {
    StagingBuffer::StagingBuffer(ResourceAllocator *allocator): allocator(allocator) {}

    StagingBuffer::StagingBuffer(const StagingBuffer& toCopy) {
        *this = toCopy;
    }

    StagingBuffer::StagingBuffer(StagingBuffer&& toMove) noexcept {
        *this = std::move(toMove);
    }

    StagingBuffer& StagingBuffer::operator=(const StagingBuffer& o) {
        if(this == &o) {
            return *this;
        }
        free();
        allocator = o.allocator;
        view = o.view;
        return *this;
    }

    StagingBuffer& StagingBuffer::operator=(StagingBuffer&& o) noexcept {
        if(this == &o) {
            return *this;
        }
        free();
        std::swap(allocator, o.allocator);
        std::swap(view, o.view);
        return *this;
    }

    void StagingBuffer::free() {
        if(!allocator) {
            return; // was moved
        }
        allocator->freeStagingBuffer(this);
    }

    StagingBuffer::~StagingBuffer() {
        free();
    }
}