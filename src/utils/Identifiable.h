//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include <cstddef>

namespace Carrot {

    using ComponentID = std::size_t;

    template<typename Type>
    struct Identifiable {
        static const ComponentID id;
    };
}

#include "Identifiable.ipp"