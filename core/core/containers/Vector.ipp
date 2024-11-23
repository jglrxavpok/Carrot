//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

#include <algorithm> // for std::sort

namespace Carrot {
#define VECTOR_TEMPLATE template<typename TElement, typename Traits> requires IsValidVectorTraits<Traits>
    VECTOR_TEMPLATE
    Vector<TElement, Traits>::Vector(Allocator& allocator): allocator(allocator) {}

    VECTOR_TEMPLATE
    Vector<TElement, Traits>::Vector(std::initializer_list<TElement> initList, Allocator& allocator): Vector(allocator) {
        *this = initList;
    }

    VECTOR_TEMPLATE
    Vector<TElement, Traits>::Vector(std::span<const TElement> initList, Allocator& allocator): Vector(allocator) {
        *this = initList;
    }

    VECTOR_TEMPLATE
    Vector<TElement, Traits>::Vector(const Vector& toCopy)
    : Vector(toCopy.allocator) {
        *this = toCopy;
    }

    VECTOR_TEMPLATE
    Vector<TElement, Traits>::Vector(Vector&& toMove) noexcept
    : Vector(toMove.allocator) {
        *this = std::move(toMove);
    }

    VECTOR_TEMPLATE
    Vector<TElement, Traits>::~Vector() {
        flush();
    }

    VECTOR_TEMPLATE
    TElement& Vector<TElement, Traits>::operator[](std::size_t index) {
        verify(index < size(), "Out-of-bounds access!");
        TElement* pData = data();
        return pData[index];
    }

    VECTOR_TEMPLATE
    const TElement& Vector<TElement, Traits>::operator[](std::size_t index) const {
        verify(index < size(), "Out-of-bounds access!");
        const TElement* pData = cdata();
        return pData[index];
    }

    VECTOR_TEMPLATE
    TElement& Vector<TElement, Traits>::get(std::size_t index) {
        return (*this)[index];
    }

    VECTOR_TEMPLATE
    const TElement& Vector<TElement, Traits>::get(std::size_t index) const {
        return (*this)[index];
    }

    VECTOR_TEMPLATE
    void Vector<TElement, Traits>::pushBack(const TElement& element) {
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
    TElement& Vector<TElement, Traits>::emplaceBack(TArg&&... arg) {
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
    void Vector<TElement, Traits>::remove(std::size_t index) {
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
    void Vector<TElement, Traits>::removeIf(const std::function<bool(const TElement&)>& predicate) {
        for(std::int64_t index = 0; index < this->elementCount;) {
            if(predicate(this->get(index))) {
                this->remove(index);
            } else {
                index++;
            }
        }
    }

    VECTOR_TEMPLATE
    void Vector<TElement, Traits>::resize(std::size_t newSize) {
        this->generationIndex++;
        if constexpr (Traits::CallConstructorAndDestructorOnResize) {
            // delete the elements that are over the new capacity (if downsize)
            for (std::size_t i = newSize; i < this->elementCount; ++i) {
                (*this)[i].~TElement();
            }
        }

        if(newSize > capacity()) {
            this->allocation = this->allocator.reallocate(this->allocation, newSize * sizeof(TElement), alignof(TElement));
        }

        if constexpr (Traits::CallConstructorAndDestructorOnResize) {
            // construct the elements that are over the old size (if upsize)
            TElement* pData = data();
            for (std::size_t i = this->elementCount; i < newSize; ++i) {
                new (&pData[i]) TElement();
            }
        }
        this->elementCount = newSize;
    }

    VECTOR_TEMPLATE
    void Vector<TElement, Traits>::setCapacity(std::size_t newSize) {
        // delete the elements that are over the new capacity
        if constexpr (Traits::CallConstructorAndDestructorOnResize) {
            for (std::size_t i = newSize; i < this->elementCount; ++i) {
                (*this)[i].~TElement();
            }
        }
        this->elementCount = std::min(this->elementCount, newSize);
        this->allocation = this->allocator.reallocate(this->allocation, newSize * sizeof(TElement), alignof(TElement));
    }

    VECTOR_TEMPLATE
    void Vector<TElement, Traits>::ensureReserve(std::size_t reserveSize) {
        if(reserveSize > capacity()) {
            grow(reserveSize);
        }
    }

    VECTOR_TEMPLATE
    void Vector<TElement, Traits>::increaseReserve(std::size_t reserveSize) {
        ensureReserve(size() + reserveSize);
    }

    VECTOR_TEMPLATE
    void Vector<TElement, Traits>::setGrowthFactor(float factor) {
        verify(factor >= 1.0f, "Growth factor cannot be < 1 !");
        this->growthFactor = factor;
    }

    VECTOR_TEMPLATE
    float Vector<TElement, Traits>::getGrowthFactor() const {
        return this->growthFactor;
    }

    VECTOR_TEMPLATE
    void Vector<TElement, Traits>::clear() {
        TElement* pData = data();
        for (std::size_t i = 0; i < this->elementCount; ++i) {
            pData[i].~TElement();
        }
        this->elementCount = 0;
    }

    VECTOR_TEMPLATE
    void Vector<TElement, Traits>::flush() {
        clear();
        this->allocator.deallocate(allocation);
        this->allocation = {};
    }

    VECTOR_TEMPLATE
    Vector<TElement, Traits>& Vector<TElement, Traits>::operator=(const Vector& o) {
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
    Vector<TElement, Traits>& Vector<TElement, Traits>::operator=(Vector&& o) noexcept {
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
    Vector<TElement, Traits>& Vector<TElement, Traits>::operator=(std::initializer_list<TElement> list) noexcept {
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
    Vector<TElement, Traits>& Vector<TElement, Traits>::operator=(std::span<const TElement> list) noexcept {
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
    void Vector<TElement, Traits>::sort(const Compare& compare) {
        std::sort(begin(), end(), compare);
    }

    VECTOR_TEMPLATE
    void Vector<TElement, Traits>::fill(const TElement& toCopy) {
        for(std::size_t i = 0; i < elementCount; i++) {
            (*this)[i] = toCopy;
        }
    }

    VECTOR_TEMPLATE
    bool Vector<TElement, Traits>::operator==(const Vector& other) const {
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
    bool Vector<TElement, Traits>::operator!=(const Vector& other) const {
        return !(*this == other);
    }

    VECTOR_TEMPLATE
    typename Vector<TElement, Traits>::template Iterator<TElement, false> Vector<TElement, Traits>::begin() {
        if(empty()) {
            return end();
        }
        return Iterator<TElement, false>(
            this, 0, this->generationIndex, false
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement, Traits>::template Iterator<TElement, false> Vector<TElement, Traits>::end() {
        return Iterator<TElement, false>(
            this, 0, this->generationIndex, true
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement, Traits>::template Iterator<const TElement, false> Vector<TElement, Traits>::begin() const {
        if(empty()) {
            return end();
        }
        return Iterator<const TElement, false>(
            (Vector*)this, 0, this->generationIndex, false
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement, Traits>::template Iterator<const TElement, false> Vector<TElement, Traits>::end() const {
        return Iterator<const TElement, false>(
            (Vector*)this, 0, this->generationIndex, true
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement, Traits>::template Iterator<TElement, true> Vector<TElement, Traits>::rbegin() {
        if(empty()) {
            return rend();
        }
        return Iterator<TElement, true>(
            this, size()-1, this->generationIndex, false
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement, Traits>::template Iterator<TElement, true> Vector<TElement, Traits>::rend() {
        return Iterator<TElement, true>(
            this, 0, this->generationIndex, true
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement, Traits>::template Iterator<const TElement, true> Vector<TElement, Traits>::rbegin() const {
        if(empty()) {
            return rend();
        }
        return Iterator<const TElement, true>(
            (Vector*)this, size()-1, this->generationIndex, false
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement, Traits>::template Iterator<const TElement, true> Vector<TElement, Traits>::rend() const {
        return Iterator<const TElement, true>(
            (Vector*)this, 0, this->generationIndex, true
        );
    }

    VECTOR_TEMPLATE
    typename Vector<TElement, Traits>::template Iterator<TElement, false> Vector<TElement, Traits>::find(const TElement& element, std::size_t startIndex) {
        for(std::size_t i = startIndex; i < elementCount; i++) {
            if((*this)[i] == element) {
                return Iterator<TElement, false>(this, i, this->generationIndex, false);
            }
        }
        return end();
    }

    VECTOR_TEMPLATE
    typename Vector<TElement, Traits>::template Iterator<const TElement, false> Vector<TElement, Traits>::find(const TElement& element, std::size_t startIndex) const {
        for(std::size_t i = startIndex; i < elementCount; i++) {
            if((*this)[i] == element) {
                return Iterator<TElement, false>((Vector*)this, i, this->generationIndex, false);
            }
        }
        return end();
    }

    VECTOR_TEMPLATE
    TElement* Vector<TElement, Traits>::data() {
        return (TElement*)this->allocation.ptr;
    }

    VECTOR_TEMPLATE
    TElement const* Vector<TElement, Traits>::data() const {
        return this->cdata();
    }

    VECTOR_TEMPLATE
    TElement const* Vector<TElement, Traits>::cdata() const {
        return (TElement const*)this->allocation.ptr;
    }

    VECTOR_TEMPLATE
    TElement& Vector<TElement, Traits>::front() {
        return *this->data();
    }

    VECTOR_TEMPLATE
    TElement& Vector<TElement, Traits>::back() {
        return *(this->data() + (this->elementCount-1));
    }

    VECTOR_TEMPLATE
    TElement const& Vector<TElement, Traits>::front() const {
        return *this->data();
    }

    VECTOR_TEMPLATE
    TElement const& Vector<TElement, Traits>::back() const {
        return *(this->data() + (this->elementCount-1));
    }

    VECTOR_TEMPLATE
    std::int64_t Vector<TElement, Traits>::size() const {
        return this->elementCount;
    }

    VECTOR_TEMPLATE
    std::int64_t Vector<TElement, Traits>::bytes_size() const {
        return this->allocation.size;
    }

    VECTOR_TEMPLATE
    std::size_t Vector<TElement, Traits>::capacity() const {
        return this->allocation.size / sizeof(TElement);
    }

    VECTOR_TEMPLATE
    bool Vector<TElement, Traits>::empty() const {
        return this->elementCount == 0;
    }

    VECTOR_TEMPLATE
    Allocator& Vector<TElement, Traits>::getAllocator() {
        return this->allocator;
    }

    VECTOR_TEMPLATE
    void Vector<TElement, Traits>::grow(std::size_t minimumCapacity) {
        std::size_t finalCapacity = static_cast<std::size_t>(std::ceil(minimumCapacity * growthFactor));
        setCapacity(std::max(finalCapacity, minimumCapacity));
    }
}