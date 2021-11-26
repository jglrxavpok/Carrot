//
// Created by jglrxavpok on 12/05/2021.
//

#pragma once

#include <algorithm>
#include <concepts>
#include <type_traits>

#define WHOLE_CONTAINER(t) t.begin(), t.end()

namespace Carrot {
    template<typename ContainerType, typename Predicate>
    inline void removeIf(ContainerType& container, Predicate predicate) {
        container.erase(std::remove_if(WHOLE_CONTAINER(container), predicate), container.end());
    }
}