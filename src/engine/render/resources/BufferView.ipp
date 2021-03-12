//
// Created by jglrxavpok on 10/03/2021.
//

#include "BufferView.h"

template<typename T>
T* Carrot::BufferView::map() {
    // TODO: proper segmentation
    return getBuffer().map<T>();
}