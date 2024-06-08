//
// Created by jglrxavpok on 08/06/2024.
//

#pragma once

#include <core/Allocator.h>

namespace Carrot {
    /**
     * Allocator that starts allocating with a "main" allocator until it reaches the main allocator limit.
     * Once reached, this allocator will start allocating through a fallback allocator
     * @tparam MainAllocator Main allocator type to use
     * @tparam Fallback Fallback allocator type to use
     */
    template<typename MainAllocator, typename Fallback>
        requires IsAllocator<MainAllocator> && IsAllocator<Fallback>
    class FallbackAllocator: public Allocator {
    public:
        MemoryBlock allocate(std::size_t size, std::size_t alignment) override {
            try {
                return main.allocate(size, alignment);
            } catch (const OutOfMemoryException& e) {
                mainNotEnough = true;
                return fallback.allocate(size, alignment);
            }
        }

        void deallocate(const MemoryBlock &block) override {
            if(mainNotEnough) {
                fallback.deallocate(block);
            } else {
                main.deallocate(block);
            }
        }

    private:
        MainAllocator main;
        Fallback fallback;

        bool mainNotEnough = false;
    };
}