//
// Created by jglrxavpok on 07/08/2022.
//

#pragma once

#include <engine/constants.h>

namespace Carrot::Render {
    template<typename T>
    struct PerFrame {
        T* data() {
            return storage.data();
        }

        PerFrame& operator=(const std::span<const T>& toCopy) {
            verify(toCopy.size() == storage.size(), "Wrong size!");
            for (i32 index = 0; index < toCopy.size(); index++) {
                storage[index] = toCopy[index];
            }
            return *this;
        }

        PerFrame& operator=(std::vector<T>&& toMove) {
            verify(toMove.size() == storage.size(), "Wrong size!");
            for (i32 index = 0; index < toMove.size(); index++) {
                storage[index] = std::move(toMove[index]);
            }
            toMove.clear();
            return *this;
        }

        T& operator[](i32 index) {
            return storage[index];
        }

        const T& operator[](i32 index) const {
            return storage[index];
        }

        i64 size() const {
            return storage.size();
        }

        template<typename T1 = T>
        requires requires(T1 a) {
            { a.clear() };
        }
        void clear() {
            for (i32 i = 0; i < storage.size(); i++) {
                storage[i].clear();
            }
        }

        template<typename T1 = T>
        requires (!requires(T1 a) {
            { a.clear() };
        })
        void clear() {
            for (i32 i = 0; i < storage.size(); i++) {
                storage[i] = {};
            }
        }

        void fill(const T& value) {
            for (i32 i = 0; i < storage.size(); i++) {
                storage[i] = value;
            }
        }

        T* begin() {
            return storage.begin();
        }

        T* end() {
            return storage.end();
        }

        const T* begin() const {
            return storage.begin();
        }

        const T* end() const {
            return storage.end();
        }

        const T* cbegin() const {
            return storage.cbegin();
        }

        const T* cend() const {
            return storage.cend();
        }

    private:
        std::array<T, MAX_FRAMES_IN_FLIGHT> storage;
    };
}