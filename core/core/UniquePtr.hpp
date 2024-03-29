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
        UniquePtr(): allocator(Allocator::getDefault()) {}

        template<typename SubType> requires std::is_base_of_v<T, SubType>
        UniquePtr(UniquePtr<SubType>&& toMove) noexcept : allocator(toMove.allocator) {
            *this = std::move(toMove);
        }

        UniquePtr(Allocator& allocator, const MemoryBlock& block)
        : allocator(allocator)
        , allocation(block)
        {}

        UniquePtr(std::nullptr_t): UniquePtr() {}

        // disallow copies
        UniquePtr(const UniquePtr&) = delete;
        UniquePtr& operator=(const UniquePtr&) = delete;

        ~UniquePtr() {
            reset();
        }

    public: // comparisons
        template<typename SubType> requires std::is_base_of_v<T, SubType>
        bool operator==(const UniquePtr& o) const {
            return allocation.ptr == o.allocation.ptr;
        }

        bool operator==(const std::nullptr_t&) const {
            return allocation.ptr == nullptr;
        }

        template<typename SubType> requires std::is_base_of_v<T, SubType>
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
            return reinterpret_cast<T*>(allocation.ptr);
        }

        const T* get() const {
            return reinterpret_cast<T*>(allocation.ptr);
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
            return *get();
        }

        const T& operator*() const {
            return *get();
        }

    public: // value changes
        void reset() {
            if(allocation.ptr) {
                reinterpret_cast<T*>(allocation.ptr)->~T();
                allocator.deallocate(allocation);
                allocation = {};
            }
        }

        UniquePtr& operator=(std::nullptr_t) {
            reset();
            return *this;
        }

        template<typename SubType> requires std::is_base_of_v<T, SubType>
        UniquePtr& operator=(UniquePtr<SubType>&& toMove)  noexcept {
            if((void*)this == (void*)&toMove) {
                return *this;
            }
            reset();
            allocation = toMove.allocation;
            toMove.allocation = {};
            return *this;
        }

    private:
        Allocator& allocator;
        MemoryBlock allocation{};

        template<class>
        friend class UniquePtr;
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