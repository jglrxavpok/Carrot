//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

namespace Carrot {
#define VECTOR_TEMPLATE template<typename TElement>
    VECTOR_TEMPLATE
    Vector<TElement>::Vector(Allocator& allocator): allocator(allocator) {}

    VECTOR_TEMPLATE
    Vector<TElement>::Vector(const Vector& toCopy)
    : Vector(toCopy.allocator) {
        *this = toCopy;
    }

    VECTOR_TEMPLATE
    Vector<TElement>::Vector(Vector&& toMove) noexcept
    : Vector(toMove.allocator) {
        *this = std::move(toMove);
    }

    VECTOR_TEMPLATE
    Vector<TElement>::~Vector() {
        flush();
    }

    VECTOR_TEMPLATE
    TElement& Vector<TElement>::operator[](std::size_t index) {
        verify(index < size(), "Out-of-bounds access!");
        TElement* pData = data();
        return pData[index];
    }

    VECTOR_TEMPLATE
    const TElement& Vector<TElement>::operator[](std::size_t index) const {
        verify(index < size(), "Out-of-bounds access!");
        const TElement* pData = cdata();
        return pData[index];
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::pushBack(const TElement& element) {
        ensureReserve(size()+1);
        generationIndex++;
        elementCount++;
        TElement* pData = data();
        new (&pData[size()-1]) TElement(element);
    }

    VECTOR_TEMPLATE
    template<typename... TArg>
    TElement& Vector<TElement>::emplaceBack(TArg&&... arg) {
        ensureReserve(size()+1);
        generationIndex++;
        elementCount++;
        TElement* pData = data();
        new (&pData[size()-1]) TElement(std::forward<TArg>(arg)...);
        return pData[size()-1];
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::remove(std::size_t index) {
        verify(index < size(), "Out of bounds!");
        generationIndex++;
        const std::size_t count = size();
        // move elements to the right of index, one step left
        TElement* pData = data();
        for(std::size_t i = index; i < count - 1; i++) {
            pData[i] = std::move(pData[i+1]);
        }
        pData[count - 1].~TElement();
        elementCount--;
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::resize(std::size_t newSize) {
        generationIndex++;
        // delete the elements that are over the new capacity (if downsize)
        for (std::size_t i = newSize; i < elementCount; ++i) {
            (*this)[i].~TElement();
        }

        if(newSize > capacity()) {
            allocation = allocator.reallocate(allocation, newSize * sizeof(TElement), alignof(TElement));
        }

        // construct the elements that are over the old size (if upsize)
        TElement* pData = data();
        for (std::size_t i = elementCount; i < newSize; ++i) {
            new (&pData[i]) TElement();
        }
        elementCount = newSize;
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::setCapacity(std::size_t newSize) {
        // delete the elements that are over the new capacity
        for (std::size_t i = newSize; i < elementCount; ++i) {
            (*this)[i].~TElement();
        }
        allocation = allocator.reallocate(allocation, newSize * sizeof(TElement), alignof(TElement));
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::ensureReserve(std::size_t reserveSize) {
        if(reserveSize > capacity()) {
            setCapacity(reserveSize);
        }
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::increaseReserve(std::size_t reserveSize) {
        ensureReserve(size() + reserveSize);
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::clear() {
        TElement* pData = data();
        for (std::size_t i = 0; i < elementCount; ++i) {
            pData[i].~TElement();
        }
        elementCount = 0;
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::flush() {
        clear();
        allocator.deallocate(allocation);
        allocation = {};
    }

    VECTOR_TEMPLATE
    Vector<TElement>& Vector<TElement>::operator=(const Vector& o) {
        clear();
        ensureReserve(o.size());
        elementCount = o.size();
        TElement* pData = data();
        for (std::size_t i = 0; i < size(); ++i) {
            new (&pData[i]) TElement(o[i]);
        }
        return *this;
    }

    VECTOR_TEMPLATE
    Vector<TElement>& Vector<TElement>::operator=(Vector&& o) noexcept {
        clear();
        elementCount = o.elementCount;
        if(allocator.isCompatibleWith(o.allocator)) {
            allocation = o.allocation;
            o.allocation = {};
            o.elementCount = 0;
        } else {
            // allocate required storage
            allocation = allocator.reallocate(allocation, o.allocation.size, alignof(TElement));
            TElement* pData = data();

            // move elements
            for (std::size_t i = 0; i < elementCount; ++i) {
                new (&pData[i]) TElement(std::move(o[i]));
            }
            o.flush(); // remove old elements from other vector
        }
        return *this;
    }

    VECTOR_TEMPLATE
    bool Vector<TElement>::operator==(const Vector& other) const {
        if(size() != other.size()) {
            return false;
        }

        for (std::size_t i = 0; i < size(); ++i) {
            if((*this)[i] != other[i]) {
                return false;
            }
        }
        return true;
    }

    VECTOR_TEMPLATE
    bool Vector<TElement>::operator!=(const Vector& other) const {
        return !(*this == other);
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<TElement, false> Vector<TElement>::begin() {
        if(empty()) {
            return end();
        }
        return Iterator<TElement, false>(
            this, 0, generationIndex
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<TElement, false> Vector<TElement>::end() {
        return Iterator<TElement, false> {};
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<const TElement, false> Vector<TElement>::begin() const {
        if(empty()) {
            return end();
        }
        return Iterator<const TElement, false>(
            (Vector*)this, 0, generationIndex
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<const TElement, false> Vector<TElement>::end() const {
        return Iterator<const TElement, false>();
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<TElement, true> Vector<TElement>::rbegin() {
        if(empty()) {
            return rend();
        }
        return Iterator<TElement, true>(
            this, size()-1, generationIndex
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<TElement, true> Vector<TElement>::rend() {
        return Iterator<TElement, true>();
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<const TElement, true> Vector<TElement>::rbegin() const {
        if(empty()) {
            return rend();
        }
        return Iterator<const TElement, true>(
            (Vector*)this, size()-1, generationIndex
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<const TElement, true> Vector<TElement>::rend() const {
        return Iterator<const TElement, true>();
    }

    VECTOR_TEMPLATE
    TElement* Vector<TElement>::data() {
        return (TElement*)allocation.ptr;
    }

    VECTOR_TEMPLATE
    TElement const* Vector<TElement>::cdata() const {
        return (TElement const*)allocation.ptr;
    }

    VECTOR_TEMPLATE
    std::size_t Vector<TElement>::size() const {
        return elementCount;
    }

    VECTOR_TEMPLATE
    std::size_t Vector<TElement>::capacity() const {
        return allocation.size / sizeof(TElement);
    }

    VECTOR_TEMPLATE
    bool Vector<TElement>::empty() const {
        return elementCount == 0;
    }

    VECTOR_TEMPLATE
    Allocator& Vector<TElement>::getAllocator() {
        return allocator;
    }
}