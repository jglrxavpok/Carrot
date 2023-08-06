//
// Created by jglrxavpok on 06/08/2023.
//

#pragma once

#include <optional>
#include <algorithm>
#include "core/utils/stringmanip.h"
#include "core/utils/Assert.h"

namespace Carrot {

    /**
     * Equivalent to std::vector, but with holes for empty slots.
     * Use when there would lots of empty space inside a vector
     *
     * Not thread safe when inserting elements
     * @tparam T array element type
     * @tparam Granularity how many elements are allocated at once
     */
    template<typename T, std::size_t Granularity = 32>
        requires std::is_default_constructible_v<T> && std::is_move_constructible_v<T>
    class SparseArray {
    public:
        SparseArray() = default;
        explicit SparseArray(std::size_t size) {
            elementCount = size;
        }

        /**
         * Returns true iif index is inside the bounds of this array AND the slot at 'index' is empty
         */
        [[nodiscard]] bool contains(std::size_t index) const {
            if(index >= elementCount) {
                return false;
            }

            const Bank* bank = getBank(index);
            if(bank == nullptr) {
                return false;
            }

            return bank->data[index % Granularity].has_value();
        }

        /**
         * Removes all non-empty elements from this array.
         */
        void clear() {
            banks.clear();
        }

        /**
         * Changes the size of this array
         * @param newSize
         */
        void resize(std::size_t newSize) {
            if(newSize == elementCount) {
                return; // nothing to do
            }

            if(newSize > elementCount) {
                // nothing to do
                elementCount = newSize;
            } else { // newSize < elementCount
                std::size_t maxBankIndex = newSize - (newSize % Granularity);
                std::erase_if(banks, [&](const Bank& b) {
                    return b.startIndex > maxBankIndex;
                });

                // in case last bank already existed
                Bank* lastBank = getBank(maxBankIndex);
                if(lastBank != nullptr) {
                    for(std::size_t i = 0; i < Granularity; i++) {
                        std::size_t index = i + maxBankIndex;
                        if(index >= newSize) {
                            if(lastBank->data[i].has_value()) {
                                lastBank->data[i].reset();
                                lastBank->allocatedCount--;
                            }
                        }
                    }
                }

                elementCount = newSize;
            }
        }

        /**
         * How many elements are present inside this array? Empty elements ARE counted inside this size.
         */
        [[nodiscard]] std::size_t size() const {
            return elementCount;
        }

        /**
         * How many non-empty elements are present inside this array?
         */
        [[nodiscard]] std::size_t sizeNonEmpty() const {
            std::size_t count = 0;

            for(const auto& b : banks) {
                count += b.allocatedCount;
            }
            return count;
        }

        /**
         * Returns the element at the given index. Allocates if there is none. Throws if index >= size()
         */
        T& operator[](std::size_t index) {
            if(index >= elementCount) {
                throw std::out_of_range(Carrot::sprintf("index (%llu) is out of bounds (size is %llu)", index, elementCount));
            }

            Bank& bank = getOrCreateBank(index);
            return bank.getOrCreate(index);
        }

        /**
         * Returns the element at the given index. Throws if there is none
         */
        T& at(std::size_t index) {
            if(index >= elementCount) {
                throw std::out_of_range(Carrot::sprintf("No element at index %llu, size is %llu", index, elementCount));
            }

            Bank* bank = getBank(index);
            if(bank == nullptr) {
                throw std::out_of_range(Carrot::sprintf("No element at index %llu, size is %llu", index, elementCount));
            }

            auto& opt = bank->data[index % Granularity];
            if(!opt.has_value()) {
                throw std::out_of_range(Carrot::sprintf("No element at index %llu, size is %llu", index, elementCount));
            }
            return opt.value();
        }

        /**
         * Returns the element at the given index. Throws if there is none
         */
        const T& at(std::size_t index) const {
            if(index >= elementCount) {
                throw std::out_of_range(Carrot::sprintf("No element at index %llu, size is %llu", index, elementCount));
            }

            Bank* bank = getBank(index);
            if(bank == nullptr) {
                throw std::out_of_range(Carrot::sprintf("No element at index %llu, size is %llu", index, elementCount));
            }

            auto& opt = bank->data[index % Granularity];
            if(!opt.has_value()) {
                throw std::out_of_range(Carrot::sprintf("No element at index %llu, size is %llu", index, elementCount));
            }
            return opt.value();
        }

    private:
        struct Bank {
            std::size_t startIndex = 0;
            std::size_t allocatedCount = 0;
            std::optional<T> data[Granularity];

            T& getOrCreate(std::size_t globalIndex) {
                verify(globalIndex >= startIndex && globalIndex < startIndex + Granularity, "Out of bounds!");

                std::size_t localIndex = (globalIndex - startIndex) % Granularity;
                auto& opt = data[localIndex];
                if(!opt.has_value()) {
                    opt = std::move(T{});
                    allocatedCount++;
                }
                return opt.value();
            }
        };

        /**
         * Get or creates bank for given index
         */
        Bank& getOrCreateBank(std::size_t index) {
            Bank* bank = getBank(index);
            if(bank == nullptr) {
                Bank& newBank = banks.emplace_back();
                newBank.startIndex = index - index % Granularity;
                std::sort(banks.begin(), banks.end(), [](const Bank& a, const Bank& b) {
                    return a.startIndex < b.startIndex;
                });
                return *getBank(index);
            } else {
                return *bank;
            }
        }

        /**
         * Get bank for given index, returns nullptr if there is none
         */
        Bank* getBank(std::size_t index) {
            std::size_t startIndex = index - index % Granularity;
            for(auto& b : banks) {
                if(b.startIndex == startIndex) {
                    return &b;
                }

                if(b.startIndex > startIndex) {
                    return nullptr;
                }
            }

            return nullptr;
        }

        /**
         * Get bank for given index, returns nullptr if there is none
         */
        const Bank* getBank(std::size_t index) const {
            std::size_t startIndex = index - index % Granularity;
            for(auto& b : banks) {
                if(b.startIndex == startIndex) {
                    return &b;
                }

                if(b.startIndex > startIndex) {
                    return nullptr;
                }
            }

            return nullptr;
        }

        std::size_t elementCount = 0;
        std::vector<Bank> banks; // sorted by startIndex
    };
}