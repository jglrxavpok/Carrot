//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include "utils/Identifiable.h"

namespace Carrot {

    struct Component {
    public:
        virtual ~Component() = default;
    };

    template<class Self>
    struct IdentifiableComponent: public Component, Identifiable<Self> {

    };

}
