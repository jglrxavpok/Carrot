//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>

namespace Carrot {

    struct MemoryBlock {
        void* ptr = nullptr;
        std::size_t size = 0;
    };

    /**
     * Abstract base class for Carrot allocators. Provides an uniform interface for allocators
     * Allocators are not thread safe by default.
     */
    struct Allocator {
        /**
         * \brief Allocates a block of memory from this allocator
         * \param size how many bytes to allocate
         * \param alignment alignment of memory to allocate
         * \return a memory block containing the allocated memory, and its size.
         * \throws OutOfMemoryException if no more memory is available
         */
        virtual MemoryBlock allocate(std::size_t size, std::size_t alignment = 1) = 0;

        /**
         * \brief Free the given memory block for this allocator. Calling 'deallocate' with an empty block (nullptr or size 0) is expected to be safe
         * \param block memory block to free
         */
        virtual void deallocate(const MemoryBlock& block) = 0;

        /**
         * \brief Attempts to reallocate a block of memory. By default, deallocates allocates a fresh block, copies to it
         * and deallocate the provided block with no thought behind it.
         * Some allocators may do something smarter, like reusing the memory used by 'block'.
         * \param block block to reallocate
         * \return a new block with the requested size
         */
        virtual MemoryBlock reallocate(const MemoryBlock& block, const std::size_t size, std::size_t alignment = 1) {
            MemoryBlock result = allocate(size, alignment);
            const std::size_t copySize = result.size < block.size ? result.size : block.size;
            if(copySize > 0) {
                memcpy(result.ptr, block.ptr, copySize);
            }
            if(block.ptr != nullptr) {
                deallocate(block);
            }
            return result;
        }

        /**
         * \brief Checks if another allocator is compatible with this allocator. This is used to know whether the memory
         * of a container can be moved to another without needing to call move-constructors.
         * \param other other allocator to check compatibility with
         * \return true iif the two allocators are compatible
         */
        virtual bool isCompatibleWith(const Allocator& other) const {
            return this == &other;
        }

        virtual ~Allocator() = default;

    public:
        static Allocator& getDefault();
        static void setDefault(Allocator&);

    private:
        static Allocator* pDefault;
    };

    class OutOfMemoryException: public std::exception {
    public:
        OutOfMemoryException() = default;

        const char* what() const noexcept override {
            return "The allocator is out of memory!";
        }
    };
}
