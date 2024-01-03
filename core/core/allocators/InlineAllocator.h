//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

#include <core/Allocator.h>

namespace Carrot {
    /**
     * \brief Allocator which holds 'MaxSize' bytes inside itself. Refuses to allocate anything else than MaxSize bytes, and only once (until deallocated).
     * \tparam MaxSize Maximum size of allocation
     */
    template<std::size_t MaxSize>
    class InlineAllocator: public Allocator {
    public:
        constexpr static std::size_t Size = MaxSize;

        MemoryBlock allocate(std::size_t size, std::size_t alignment = 1) override;

        void deallocate(const MemoryBlock& block) override;

    private:
        std::uint8_t pData[MaxSize];
        bool alreadyAllocated = false;
    };

} // Carrot

#include <core/allocators/InlineAllocator.ipp>
