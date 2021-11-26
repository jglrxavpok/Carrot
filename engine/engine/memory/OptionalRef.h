//
// Created by jglrxavpok on 18/09/2021.
//

#pragma once

#include <engine/utils/Macros.h>

namespace Carrot::Memory {
    /// Optional wrapper for a pointer, which will act as a T&
    template<typename T>
    class OptionalRef {
    public:
        OptionalRef() = default;

        OptionalRef(T& value): value(&value) {}
        OptionalRef(T* value): value(value) {}

        bool hasValue() const {
            return value != nullptr;
        }

        explicit operator bool() const {
            return hasValue();
        }

        T& asRef() {
            verify(hasValue(), "No value in optional!");
            return *value;
        }

        T& asRef() const {
            verify(hasValue(), "No value in optional!");
            return *value;
        }

        operator T&() {
            return asRef();
        }

        operator const T&() const {
            return asRef();
        }

        T* asPtr() {
            verify(hasValue(), "No value in optional!");
            return value;
        }

        T* asPtr() const {
            verify(hasValue(), "No value in optional!");
            return value;
        }

        T* operator->() {
            return asPtr();
        }

        T* operator->() const {
            return asPtr();
        }

    private:
        T* value = nullptr;
    };
}