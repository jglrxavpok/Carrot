//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

#include <core/Allocator.h>
#include <core/Macros.h>
#include <span>
#include <initializer_list>

namespace Carrot {
    struct DefaultVectorTraits {
        /// Should Vector call the constructor and destructors on resizes?
        /// Setting this to false is only allowed for trivially constructible types
        constexpr static bool CallConstructorAndDestructorOnResize = true;
    };

    template<typename TTrait>
    concept IsValidVectorTraits = requires()
    {
        { TTrait::CallConstructorAndDestructorOnResize } -> std::convertible_to<bool>;
    };

    struct NoConstructorVectorTraits
    {
        constexpr static bool CallConstructorAndDestructorOnResize = false;
    };

    static_assert(IsValidVectorTraits<NoConstructorVectorTraits>);

    /**
     * \brief Contiguous array of elements, which can be dynamically resized.
     * \tparam TElement type of element to store
     */
    template<typename TElement, typename VectorTraits = DefaultVectorTraits>
        requires IsValidVectorTraits<VectorTraits>
    class Vector {
    public:
        static_assert(sizeof(TElement) > 0, "Special handling would need to be added to support 0-length elements inside Vector");
        static_assert(std::is_trivial_v<TElement> || VectorTraits::CallConstructorAndDestructorOnResize, "CallConstructorAndDestructorOnResize=false is only allowed for trivial types");

        /**
         * \brief Creates an empty Vector and sets its corresponding allocator.
         * \param allocator allocator to use for this vector. Allocator::getDefault() by default
         */
        explicit Vector(Allocator& allocator = Allocator::getDefault());
        Vector(std::initializer_list<TElement> initList, Allocator& allocator = Allocator::getDefault());
        Vector(std::span<const TElement> initList, Allocator& allocator = Allocator::getDefault());
        Vector(const Vector& toCopy);
        Vector(Vector&& toMove) noexcept;
        ~Vector();

    public: // access
        TElement& operator[](std::size_t index);
        const TElement& operator[](std::size_t index) const;
        TElement& get(std::size_t index);
        const TElement& get(std::size_t index) const;

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
         * \brief Sets the growth factor of this vector: used to know how much capacity must be reserved when the vector increases in size past its capacity.
         * By default, set to 1.0f, and pushBack and emplaceBack will reallocate each time an item is added (if no setCapacity call was done prior).
         * \param factor factor, starting from 1.0f
         */
        void setGrowthFactor(float factor);

        float getGrowthFactor() const;

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
        Vector& operator=(std::initializer_list<TElement> o) noexcept;
        Vector& operator=(std::span<const TElement> o) noexcept;

        /**
         * \brief Sort the vector based on a given comparison function
         * \tparam Compare type of compare parameter
         * \param compare comparison object, can be an object, a function, a lambda, whatever. Needs `compare(const TElement& a, const TElement& b)` compiling and returning a boolean whether a < b (like std::less)
         */
        template<typename Compare>
        void sort(const Compare& compare);

        /**
         * \brief Replaces all elements of this vector with the given value
         * \param toCopy value to use for all elements of this vector
         */
        void fill(const TElement& toCopy);

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
            Iterator(Vector* pVector, std::size_t elementIndex, std::size_t generationIndex, bool isEnd)
                : pVector(pVector)
                , elementIndex(elementIndex)
                , generationIndex(generationIndex)
                , isEnd(isEnd)
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
                if(isValid()) {
                    verify(pVector->generationIndex == generationIndex, "Vector was modified, cannot use this iterator anymore!");
                    if constexpr (Reverse) {
                        if(elementIndex == pVector->size() - 1) {
                            isEnd = true;
                            return *this;
                        }
                        const std::size_t nextIndex = elementIndex + 1;
                        elementIndex = nextIndex;
                    } else {
                        verify(elementIndex > 0, "Cannot get prev element of head!");
                        const std::size_t nextIndex = elementIndex - 1;
                        elementIndex = nextIndex;
                    }
                } else {
                    isEnd = false;
                    if constexpr (Reverse) {
                        elementIndex = 0;
                    } else {
                        elementIndex = pVector->size()-1;
                    }
                }
                return *this;
            }

            Iterator& next() {
                if(isValid()) {
                    verify(pVector->generationIndex == generationIndex, "Vector was modified, cannot use this iterator anymore!");
                    if constexpr (Reverse) {
                        if(elementIndex == 0) {
                            isEnd = true;
                            return *this;
                        }
                        const std::size_t nextIndex = elementIndex - 1;
                        elementIndex = nextIndex;
                    } else {
                        const std::size_t nextIndex = elementIndex + 1;
                        if(nextIndex >= pVector->size()) {
                            // reached the end
                            isEnd = true;
                            return *this;
                        }
                        elementIndex = nextIndex;
                    }
                } else {
                    verify(false, "Cannot call next on end iterator!");
                }
                return *this;
            }

            Iterator& operator++() {
                return next();
            }

            Iterator operator++(int) {
                Iterator tmp = *this;
                return tmp.next();
            }

            Iterator& operator--() {
                return prev();
            }

            Iterator operator--(int) {
                Iterator tmp = *this;
                return tmp.prev();
            }

            difference_type operator-(const Iterator& o) const {
                difference_type r = 0;
                if(o.isValid()) {
                    if(isValid()) {
                        r = elementIndex;
                        r -= o.elementIndex;
                    } else {
                        r = -o.elementIndex;
                        if constexpr (!Reverse) {
                            r += o.pVector->size();
                        } else {
                            r--; // account for the fact that the end reverse iterator ends up "logically" at -1
                        }
                    }
                } else if(isValid()) {
                    if(o.isValid()) {
                        verify(false, "unreachable");
                    } else {
                        r = elementIndex;
                        if constexpr (!Reverse) {
                            r -= pVector->size();
                        } else {
                            r++; // account for the fact that the end reverse iterator ends up "logically" at -1
                        }
                    }
                }
                if constexpr (Reverse) {
                    return -r;
                }
                return r;
            }

            Iterator operator+(std::ptrdiff_t offset) const {
                if(isEndIterator()) { // end iterator
                    verify(offset <= 0, "Cannot go past an end iterator!");
                    verify(offset >= -(static_cast<std::ptrdiff_t>(this->pVector->size())), "Out of bounds access!");
                    if(offset == 0) {
                        return *this;
                    }
                    if constexpr (Reverse) {
                        return Iterator(pVector, -offset-1, generationIndex, false);
                    }
                    return Iterator(pVector, pVector->size()+offset, generationIndex, false);
                }
                std::int64_t newIndex = elementIndex;
                if constexpr (Reverse) {
                    newIndex -= offset;
                    verify(newIndex >= -1 && newIndex < this->pVector->size(), "Out of bounds access!");
                    if(newIndex == -1) {
                        return Iterator(pVector, 0, generationIndex, true); // end iterator
                    }
                    return Iterator(pVector, newIndex, generationIndex, false);
                } else {
                    newIndex += offset;
                    verify(newIndex >= 0 && newIndex <= this->pVector->size(), "Out of bounds access!");
                    if(newIndex == this->pVector->size()) {
                        return Iterator(pVector, newIndex, generationIndex, true); // end iterator
                    }
                    return Iterator(pVector, newIndex, generationIndex, false);
                }
            }

            Iterator operator-(std::ptrdiff_t offset) const {
                return *this + (-offset);
            }

            bool operator<(const Iterator& o) const {
                return (*this - o) < 0;
            }

            bool operator>(const Iterator& o) const {
                return !(*this < o) && *this != o;
            }

            bool operator<=(const Iterator& o) const {
                return !(*this > o);
            }

            bool operator>=(const Iterator& o) const {
                return !(*this < o);
            }

            bool operator==(const Iterator& o) const {
                if(o.isValid() != isValid()) {
                    return false;
                }
                if(!o.isValid()) {
                    return true;
                }
                verify(pVector == o.pVector, "Cannot compare iterators of different vectors!");
                verify(generationIndex == o.generationIndex, "Cannot compare iterators after modification of the vector!");
                return pVector == o.pVector && elementIndex == o.elementIndex;
            }

            bool operator!=(const Iterator& o) const {
                return !(*this == o);
            }

            std::size_t getIndex() const {
                return elementIndex;
            }

            bool isEndIterator() const {
                return isEnd;
            }

            bool isValid() const {
                return !isEndIterator();
            }

        private:
            Vector* pVector = nullptr;
            std::size_t elementIndex = 0;
            std::size_t generationIndex = 0;
            bool isEnd = true;
        };

        Iterator<TElement, false> begin();
        Iterator<TElement, false> end();

        Iterator<const TElement, false> begin() const;
        Iterator<const TElement, false> end() const;

        Iterator<TElement, true> rbegin();
        Iterator<TElement, true> rend();

        Iterator<const TElement, true> rbegin() const;
        Iterator<const TElement, true> rend() const;

        Iterator<TElement, false> find(const TElement& element, std::size_t startIndex = 0);
        Iterator<const TElement, false> find(const TElement& element, std::size_t startIndex = 0) const;

        /**
         * Pointer to first element (all elements are contiguous)
         */
        TElement* data();

        /**
         * Pointer to first element (const version)
         */
        TElement const* data() const;

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
        std::int64_t size() const;

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
        void grow(std::size_t minimumCapacity);

        Allocator& allocator;
        MemoryBlock allocation;

        float growthFactor = 1.0f;
        std::size_t elementCount = 0;
        std::size_t generationIndex = 0; // used to invalidate iterators
    };

}

#include <core/containers/Vector.ipp>
