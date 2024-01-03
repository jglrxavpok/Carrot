//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

#include <core/Allocator.h>
#include <core/Macros.h>
#include <span>

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

        /**
         * \brief Sort the vector based on a given comparison function
         * \tparam Compare type of compare parameter
         * \param compare comparison object, can be an object, a function, a lambda, whatever. Needs `compare(const TElement& a, const TElement& b)` compiling and returning a boolean whether a < b (like std::less)
         */
        template<typename Compare>
        void sort(const Compare& compare);

    public:
        bool operator==(const Vector& other) const;
        bool operator!=(const Vector& other) const;

    public: // iteration
        template<typename T, bool Reverse>
        class Iterator {
        public:
            using difference_type = std::ptrdiff_t;
            using value_type = T;

            Iterator() {}
            Iterator(Vector* pVector, std::size_t elementIndex, std::size_t generationIndex)
                : pVector(pVector)
                , elementIndex(elementIndex)
                , generationIndex(generationIndex)
            {}

            T& operator*() const {
                verify(pVector, "Cannot derefence end iterator!");
                return (*pVector)[elementIndex];
            }

            T* operator->() const {
                verify(pVector, "Cannot derefence end iterator!");
                return &(*pVector)[elementIndex];
            }

            Iterator& prev() {
                if(pVector) {
                    verify(pVector->generationIndex == generationIndex, "Vector was modified, cannot use this iterator anymore!");
                    if constexpr (Reverse) {
                        if(elementIndex == pVector->size() - 1) {
                            pVector = nullptr;
                            return *this;
                        }
                        const std::size_t nextIndex = elementIndex + 1;
                        elementIndex = nextIndex;
                    } else {
                        verify(elementIndex > 0, "Cannot get prev element of head!");
                        const std::size_t nextIndex = elementIndex - 1;
                        elementIndex = nextIndex;
                    }
                }
                return *this;
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
                    } else {
                        const std::size_t nextIndex = elementIndex + 1;
                        if(nextIndex >= pVector->size()) {
                            // reached the end
                            pVector = nullptr;
                            return *this;
                        }
                        elementIndex = nextIndex;
                    }
                }
                return *this;
            }

            Iterator& operator++() {
                return next();
            }

            Iterator operator++(int) {
                return next();
            }

            Iterator& operator--() {
                return prev();
            }

            Iterator operator--(int) {
                return prev();
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

            bool operator!=(const Iterator& o) const {
                return !(*this == o);
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

        operator std::span<TElement>() {
            return std::span(data(), size());
        }

        operator std::span<const TElement>() const {
            return std::span(cdata(), size());
        }

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
