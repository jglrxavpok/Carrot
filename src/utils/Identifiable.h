//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include <cstddef>
#include <ecs/Signature.hpp>

namespace Carrot {
    template<typename Type>
    struct Identifiable {
        static const ComponentID id;
    };
}

#include "Identifiable.ipp"