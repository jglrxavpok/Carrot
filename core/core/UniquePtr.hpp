//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

#include <core/Allocator.h>
#include <core/allocators/MallocAllocator.h>

namespace Carrot {
    /**
     * \brief Allocator-aware version of std::unique_ptr, but no deleter
     * \tparam T type of element to store
     */
    template<typename T>
    class UniquePtr {
    public:
        UniquePtr(): allocator(MallocAllocator::instance) {}
        UniquePtr(UniquePtr&& toMove) noexcept : allocator(toMove.allocator) {
            *this = std::move(toMove);
        }

        UniquePtr(Allocator& allocator, const MemoryBlock& block)
        : allocator(allocator)
        , allocation(block)
        {}

        // disallow copies
        UniquePtr(const UniquePtr&) = delete;
        UniquePtr& operator=(const UniquePtr&) = delete;

        ~UniquePtr() {
            reset();
        }

    public: // comparisons
        bool operator==(const UniquePtr& o) const {
            return allocation.ptr == o.allocation.ptr;
        }

        bool operator==(const std::nullptr_t&) const {
            return allocation.ptr == nullptr;
        }

        bool operator!=(const UniquePtr& o) const {
            return !(*this == o);
        }

        bool operator!=(const std::nullptr_t&) const {
            return allocation.ptr != nullptr;
        }

        explicit operator bool() const {
            return *this != nullptr;
        }

    public: // get value
        T* get() {
            return static_cast<T*>(allocation.ptr);
        }

        const T* get() const {
            return static_cast<T*>(allocation.ptr);
        }

        operator T*() {
            return get();
        }

        operator const T*() const {
            return get();
        }

        T* operator->() {
            return get();
        }

        const T* operator->() const {
            return get();
        }

        T& operator*() {
            return get();
        }

        const T& operator*() const {
            return get();
        }

    public: // value changes
        void reset() {
            if(allocation.ptr) {
                static_cast<T*>(allocation.ptr)->~T();
                allocator.deallocate(allocation);
                allocation = {};
            }
        }

        UniquePtr& operator=(std::nullptr_t) {
            reset();
            return *this;
        }

        UniquePtr& operator=(UniquePtr&& toMove)  noexcept {
            if(&toMove == this) {
                return *this;
            }
            reset();
            allocation.ptr = toMove.allocation.ptr;
            toMove.allocation = {};
            return *this;
        }

    private:
        Allocator& allocator;
        MemoryBlock allocation;
    };

    /**
     * \brief Creates a new UniquePtr containing a TElement, constructed with the arguments provided.
     * \tparam TElement type of element to create
     * \tparam TArg arguments to construct the element
     * \param allocator allocator to use for allocating this element
     * \param arg arguments to construct the element
     * \return UniquePtr containing the constructed element
     */
    template<typename TElement, typename... TArg>
    UniquePtr<TElement> makeUnique(Allocator& allocator, TArg&&... arg) {
        MemoryBlock block = allocator.allocate(sizeof(TElement), alignof(TElement));
        UniquePtr<TElement> ptr{allocator, block};
        new(block.ptr) TElement(std::forward<TArg>(arg)...);
        return ptr;
    }
}