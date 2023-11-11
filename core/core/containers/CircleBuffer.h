//
// Created by jglrxavpok on 29/10/2023.
//

#pragma once

#include <cstdint>
#include <array>

namespace Carrot {

    /**
     * Circle buffer: array of maximum MaxSize elements, replacing old entries once limit is hit
     * @tparam ElementType type of elements inside the array
     * @tparam MaxSize maximum count of elements inside this buffer
     */
    template<typename ElementType, std::int64_t MaxSize>
    class CircleBuffer {
        static_assert(MaxSize > 0, "Circle buffer cannot be <0 or 0-length");

    public:
        constexpr static std::int64_t MaxCount = MaxSize;

        CircleBuffer() {}

        /**
         * Gets the element at 'index'
         * For convenience, index is modulo-ed with MaxCount to ensure index always lands inside circle buffer
         */
        ElementType& operator[](std::int64_t index) {
            while(index < 0) {
                index += MaxSize;
            }
            return elements[index % MaxSize];
        }

        /**
         * Gets the element at 'index'
         * For convenience, index is modulo-ed with MaxCount to ensure index always lands inside circle buffer
         */
        const ElementType& operator[](std::int64_t index) const {
            while(index < 0) {
                index += MaxSize;
            }
            return elements[index % MaxSize];
        }

        /**
         * Copies 'element' inside this circle buffer
         */
        void push(const ElementType& element) {
            next() = element;
        }

        /**
         * Moves 'element' inside this circle buffer
         */
        void emplace(ElementType&& element) {
            next() = std::move(element);
        }

        /**
         * Current position inside the buffer
         * @return
         */
        std::int64_t getCurrentPosition() const {
            return position;
        }

        /**
         * How many elements are inside this buffer?
         * 0 when initialized, 'MaxSize' once filled
         */
        std::int64_t getCount() const {
            return count;
        }

    private:
        ElementType& next() {
            ElementType& r = elements[position];
            position = (position + 1) % MaxSize;
            count = std::min(MaxSize, count + 1);
            return r;
        }

        std::array<ElementType, MaxSize> elements;
        std::int64_t count = 0;
        std::int64_t position = 0;
    };

} // Carrot
