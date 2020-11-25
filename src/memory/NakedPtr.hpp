//
// Created by jglrxavpok on 21/11/2020.
//

#pragma once

template<typename PointedType>
class NakedPtr {
public:
    NakedPtr(): pointer(nullptr) {}
    NakedPtr(PointedType* ptr): pointer(ptr) {}
    NakedPtr(PointedType*& ptr): pointer(ptr) {}
    NakedPtr(NakedPtr<PointedType>& ptr): pointer(ptr.pointer) {}

    PointedType operator->() const {
        static_assert(pointer != nullptr, "Pointer must not be nullptr!");
        return *pointer;
    }

    PointedType operator->() {
        static_assert(pointer != nullptr, "Pointer must not be nullptr!");
        return *pointer;
    }

    PointedType* get() const {
        return pointer;
    }

    [[nodiscard]] bool isNull() const {
        return !pointer;
    }

    void reset(PointedType* newValue = nullptr) {
        pointer = newValue;
    }

private:
    PointedType* pointer = nullptr;
};


