//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

#include <core/Allocator.h>
#include <core/Macros.h>

namespace Carrot {
    /**
     * \brief Contiguous array of elements, which can be dynamically resized.
     * \tparam TElement type of element to store
     */
    template<typename TElement>
    class Vector {
    public:
        static_assert(sizeof(TElement) > 0, "Special handling would need to be added to support 0-length elements inside Vector");

        /**
         * \brief Creates an empty Vector and sets its corresponding allocator.
         * \param allocator allocator to use for this vector. MallocAllocator by default
         */
        explicit Vector(Allocator& allocator = MallocAllocator::instance);
        Vector(const Vector& toCopy);
        Vector(Vector&& toMove) noexcept;
        ~Vector();

    public: // access
        TElement& operator[](std::size_t index);
        const TElement& operator[](std::size_t index) const;

    public: // modifications to the vector
        /**
         * \brief Adds a copy of the given element at the end of this vector
         * \param element element to copy at the end of this vector
         */
        void pushBack(const TElement& element);

        /**
         * \brief Constructs an element at the end of this vector, with the given arguments
         * \tparam TArg Type of constructor arguments
         * \param arg constructor arguments
         * \return newly created element. Reference is valid until this vector is modified
         */
        template<typename... TArg>
        TElement& emplaceBack(TArg&&... arg);

        /**
         * \brief Removes an element based on its index. Does not change capacity, so no allocations, but requires a move of each element following this index
         * \param index Index of element to remove
         */
        void remove(std::size_t index);

        /**
         * \brief Sets the size of this vector, removing or adding elements to fit the new size
         */
        void resize(std::size_t newSize);

        /**
         * \brief Sets the capacity of this vector, removing elements if needed to fit the new size
         */
        void setCapacity(std::size_t newSize);

        /**
         * \brief Modifies the capacity of this vector to fit at least 'reserveSize' elements.
         * If the vector can already fit more than 'reserveSize' elements inside its capacity, does nothing
         * \param reserveSize size to ensure
         */
        void ensureReserve(std::size_t reserveSize);

        /**
         * \brief Modifies the capacity of this vector to fit at least size() + 'reserveSize' elements.
         * If the vector can already fit more than size() + 'reserveSize' elements inside its capacity, does nothing
         * \param reserveSize size to ensure
         */
        void increaseReserve(std::size_t reserveSize);

        /**
         * \brief Removes elements of this vector, without modifying its capacity (no deallocations)
         */
        void clear();

        /**
         * \brief Removes elements of this vector, and deallocates backing storage.
         * After this call, both size and capacity are 0
         */
        void flush();

        Vector& operator=(const Vector& o);

        Vector& operator=(Vector&& o) noexcept;

    public:
        bool operator==(const Vector& other) const;
        bool operator!=(const Vector& other) const;

    public: // iteration
        template<typename T, bool Reverse>
        class Iterator {
        public:
            Iterator() {}
            Iterator(Vector* pVector, std::size_t elementIndex, std::size_t generationIndex)
                : pVector(pVector)
                , elementIndex(elementIndex)
                , generationIndex(generationIndex)
            {}

            T& operator*() {
                verify(pVector, "Cannot derefence end iterator!");
                return (*pVector)[elementIndex];
            }

            T* operator->() {
                verify(pVector, "Cannot derefence end iterator!");
                return &(*pVector)[elementIndex];
            }

            Iterator& next() {
                if(pVector) {
                    verify(pVector->generationIndex == generationIndex, "Vector was modified, cannot use this iterator anymore!");
                    if constexpr (Reverse) {
                        if(elementIndex == 0) {
                            pVector = nullptr;
                            return *this;
                        }
                        const std::size_t nextIndex = elementIndex - 1;
                        elementIndex = nextIndex;
                        return *this;
                    } else {
                        const std::size_t nextIndex = elementIndex + 1;
                        if(nextIndex >= pVector->size()) {
                            // reached the end
                            pVector = nullptr;
                            return *this;
                        }
                        elementIndex = nextIndex;
                        return *this;
                    }
                } else { // end iterator, nothing to do
                    return *this;
                }
            }

            Iterator& operator++() {
                return next();
            }

            bool operator==(const Iterator& o) const {
                if(o.pVector == nullptr) {
                    return pVector == nullptr;
                }
                if(pVector == nullptr) {
                    return o.pVector == nullptr;
                }
                verify(generationIndex == o.generationIndex, "Cannot compare iterators after modification of the vector!");
                return pVector == o.pVector && elementIndex == o.elementIndex;
            }

            std::size_t getIndex() const {
                return elementIndex;
            }

        private:
            Vector* pVector = nullptr;
            std::size_t elementIndex;
            std::size_t generationIndex;
        };

        Iterator<TElement, false> begin();
        Iterator<TElement, false> end();

        Iterator<const TElement, false> begin() const;
        Iterator<const TElement, false> end() const;

        Iterator<TElement, true> rbegin();
        Iterator<TElement, true> rend();

        Iterator<const TElement, true> rbegin() const;
        Iterator<const TElement, true> rend() const;

        /**
         * Pointer to first element (all elements are contiguous)
         */
        TElement* data();

        /**
         * Pointer to first element (const version)
         */
        TElement const* cdata() const;

    public: // queries
        /**
         * How many elements are inside this vector?
         */
        std::size_t size() const;

        /**
         * Is this vector empty? (ie size == 0)
         */
        bool empty() const;

        /**
         * How many elements can be fit inside this vector, given its current allocations.
         */
        std::size_t capacity() const;

        Allocator& getAllocator();

    private:
        Allocator& allocator;
        MemoryBlock allocation;

        std::size_t elementCount = 0;
        std::size_t generationIndex = 0; // used to invalidate iterators
    };

}
#include <core/containers/Vector.ipp>

// to make it compatible with std::span
// TODO
/*
static_assert(std::ranges::range<Carrot::Vector<int>>);
static_assert(std::ranges::contiguous_range<Carrot::Vector<int>>);
static_assert(std::ranges::sized_range<Carrot::Vector<int>>);
*/