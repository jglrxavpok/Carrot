//
// Created by jglrxavpok on 10/03/2021.
//

#include "BufferView.h"

template<typename T>
T* Carrot::BufferView::map() {
    // TODO: proper segmentation
    std::uint8_t* pData = reinterpret_cast<std::uint8_t*>(mapGeneric());
    pData += start;
    return reinterpret_cast<T*>(pData);
}