//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

#include <algorithm> // for std::sort

namespace Carrot {
#define VECTOR_TEMPLATE template<typename TElement>
    VECTOR_TEMPLATE
    Vector<TElement>::Vector(Allocator& allocator): allocator(allocator) {}

    VECTOR_TEMPLATE
    Vector<TElement>::Vector(std::initializer_list<TElement> initList, Allocator& allocator): Vector(allocator) {
        *this = initList;
    }

    VECTOR_TEMPLATE
    Vector<TElement>::Vector(std::span<const TElement> initList, Allocator& allocator): Vector(allocator) {
        *this = initList;
    }

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
    TElement& Vector<TElement>::get(std::size_t index) {
        return (*this)[index];
    }

    VECTOR_TEMPLATE
    const TElement& Vector<TElement>::get(std::size_t index) const {
        return (*this)[index];
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::pushBack(const TElement& element) {
        if(this->size() >= this->capacity()) {
            this->grow(size()+1);
        }
        this->generationIndex++;
        this->elementCount++;
        TElement* pData = data();
        new (&pData[size()-1]) TElement(element);
    }

    VECTOR_TEMPLATE
    template<typename... TArg>
    TElement& Vector<TElement>::emplaceBack(TArg&&... arg) {
        if(this->size() >= this->capacity()) {
            this->grow(size()+1);
        }
        this->generationIndex++;
        this->elementCount++;
        TElement* pData = data();
        new (&pData[size()-1]) TElement(std::forward<TArg>(arg)...);
        return pData[size()-1];
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::remove(std::size_t index) {
        verify(index < size(), "Out of bounds!");
        this->generationIndex++;
        const std::size_t count = size();
        // move elements to the right of index, one step left
        TElement* pData = data();
        for(std::size_t i = index; i < count - 1; i++) {
            pData[i] = std::move(pData[i+1]);
        }
        pData[count - 1].~TElement();
        this->elementCount--;
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::resize(std::size_t newSize) {
        this->generationIndex++;
        // delete the elements that are over the new capacity (if downsize)
        for (std::size_t i = newSize; i < this->elementCount; ++i) {
            (*this)[i].~TElement();
        }

        if(newSize > capacity()) {
            this->allocation = this->allocator.reallocate(this->allocation, newSize * sizeof(TElement), alignof(TElement));
        }

        // construct the elements that are over the old size (if upsize)
        TElement* pData = data();
        for (std::size_t i = this->elementCount; i < newSize; ++i) {
            new (&pData[i]) TElement();
        }
        this->elementCount = newSize;
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::setCapacity(std::size_t newSize) {
        // delete the elements that are over the new capacity
        for (std::size_t i = newSize; i < this->elementCount; ++i) {
            (*this)[i].~TElement();
        }
        this->elementCount = std::min(this->elementCount, newSize);
        this->allocation = this->allocator.reallocate(this->allocation, newSize * sizeof(TElement), alignof(TElement));
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
    void Vector<TElement>::setGrowthFactor(float factor) {
        verify(factor >= 1.0f, "Growth factor cannot be < 1 !");
        this->growthFactor = factor;
    }

    VECTOR_TEMPLATE
    float Vector<TElement>::getGrowthFactor() const {
        return this->growthFactor;
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::clear() {
        TElement* pData = data();
        for (std::size_t i = 0; i < this->elementCount; ++i) {
            pData[i].~TElement();
        }
        this->elementCount = 0;
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::flush() {
        clear();
        this->allocator.deallocate(allocation);
        this->allocation = {};
    }

    VECTOR_TEMPLATE
    Vector<TElement>& Vector<TElement>::operator=(const Vector& o) {
        clear();
        ensureReserve(o.size());
        this->elementCount = o.size();
        TElement* pData = data();
        for (std::size_t i = 0; i < size(); ++i) {
            new (&pData[i]) TElement(o[i]);
        }
        return *this;
    }

    VECTOR_TEMPLATE
    Vector<TElement>& Vector<TElement>::operator=(Vector&& o) noexcept {
        clear();
        this->elementCount = o.elementCount;
        if(this->allocator.isCompatibleWith(o.allocator)) {
            this->allocation = o.allocation;
            o.allocation = {};
            o.elementCount = 0;
        } else {
            // allocate required storage
            this->allocation = this->allocator.reallocate(allocation, o.allocation.size, alignof(TElement));
            TElement* pData = data();

            // move elements
            for (std::size_t i = 0; i < this->elementCount; ++i) {
                new (&pData[i]) TElement(std::move(o[i]));
            }
            o.flush(); // remove old elements from other vector
        }
        return *this;
    }

    VECTOR_TEMPLATE
    Vector<TElement>& Vector<TElement>::operator=(std::initializer_list<TElement> list) noexcept {
        clear();
        ensureReserve(list.size());
        this->elementCount = list.size();
        TElement* pData = data();
        const TElement* pListData = list.begin();
        for (std::size_t i = 0; i < size(); ++i) {
            new (&pData[i]) TElement(pListData[i]);
        }
        return *this;
    }

    VECTOR_TEMPLATE
    Vector<TElement>& Vector<TElement>::operator=(std::span<const TElement> list) noexcept {
        clear();
        ensureReserve(list.size());
        this->elementCount = list.size();
        TElement* pData = data();
        const TElement* pListData = list.data();
        for (std::size_t i = 0; i < size(); ++i) {
            new (&pData[i]) TElement(pListData[i]);
        }
        return *this;
    }

    VECTOR_TEMPLATE
    template<typename Compare>
    void Vector<TElement>::sort(const Compare& compare) {
        std::sort(begin(), end(), compare);
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::fill(const TElement& toCopy) {
        for(std::size_t i = 0; i < elementCount; i++) {
            (*this)[i] = toCopy;
        }
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
            this, 0, this->generationIndex, false
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<TElement, false> Vector<TElement>::end() {
        return Iterator<TElement, false>(
            this, 0, this->generationIndex, true
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<const TElement, false> Vector<TElement>::begin() const {
        if(empty()) {
            return end();
        }
        return Iterator<const TElement, false>(
            (Vector*)this, 0, this->generationIndex, false
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<const TElement, false> Vector<TElement>::end() const {
        return Iterator<const TElement, false>(
            (Vector*)this, 0, this->generationIndex, true
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<TElement, true> Vector<TElement>::rbegin() {
        if(empty()) {
            return rend();
        }
        return Iterator<TElement, true>(
            this, size()-1, this->generationIndex, false
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<TElement, true> Vector<TElement>::rend() {
        return Iterator<TElement, true>(
            this, 0, this->generationIndex, true
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<const TElement, true> Vector<TElement>::rbegin() const {
        if(empty()) {
            return rend();
        }
        return Iterator<const TElement, true>(
            (Vector*)this, size()-1, this->generationIndex, false
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<const TElement, true> Vector<TElement>::rend() const {
        return Iterator<const TElement, true>(
            (Vector*)this, 0, this->generationIndex, true
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<TElement, false> Vector<TElement>::find(const TElement& element, std::size_t startIndex) {
        for(std::size_t i = startIndex; i < elementCount; i++) {
            if((*this)[i] == element) {
                return Iterator<TElement, false>(this, i, this->generationIndex, false);
            }
        }
        return end();
    }

    VECTOR_TEMPLATE
    typename Vector<TElement>::template Iterator<const TElement, false> Vector<TElement>::find(const TElement& element, std::size_t startIndex) const {
        for(std::size_t i = startIndex; i < elementCount; i++) {
            if((*this)[i] == element) {
                return Iterator<TElement, false>((Vector*)this, i, this->generationIndex, false);
            }
        }
        return end();
    }

    VECTOR_TEMPLATE
    TElement* Vector<TElement>::data() {
        return (TElement*)this->allocation.ptr;
    }

    VECTOR_TEMPLATE
    TElement const* Vector<TElement>::cdata() const {
        return (TElement const*)this->allocation.ptr;
    }

    VECTOR_TEMPLATE
    std::int64_t Vector<TElement>::size() const {
        return this->elementCount;
    }

    VECTOR_TEMPLATE
    std::size_t Vector<TElement>::capacity() const {
        return this->allocation.size / sizeof(TElement);
    }

    VECTOR_TEMPLATE
    bool Vector<TElement>::empty() const {
        return this->elementCount == 0;
    }

    VECTOR_TEMPLATE
    Allocator& Vector<TElement>::getAllocator() {
        return this->allocator;
    }

    VECTOR_TEMPLATE
    void Vector<TElement>::grow(std::size_t minimumCapacity) {
        std::size_t finalCapacity = static_cast<std::size_t>(std::ceil(minimumCapacity * growthFactor));
        setCapacity(std::max(finalCapacity, minimumCapacity));
    }
}