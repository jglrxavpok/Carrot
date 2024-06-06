//
// Created by jglrxavpok on 06/06/2024.
//

#pragma once
#include <core/async/Locks.h>
#include <core/containers/Vector.hpp>

namespace Carrot::Render {

    /// Pool of binary semaphores, intended for use in a single frame
    /// All semaphores inside this pool are owned by the pool
    /// Thread-safe (but not lock-free!)
    class SemaphorePool {
    public:
        explicit SemaphorePool();

        /// Resets the entire pool (ie get() will start giving semaphores that already exist)
        void reset();

        /// Gets a free semaphore from this pool and allocates it
        vk::Semaphore get();
    private:
        struct Block {
            constexpr static std::size_t Size = 1024;
            std::array<vk::UniqueSemaphore, Size> semaphores{};
        };
        Carrot::Async::SpinLock lock;
        Carrot::Vector<Block> blocks;
        std::size_t cursor = 0;
    };

} // Carrot::Render
