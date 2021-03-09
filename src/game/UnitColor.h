//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include "engine/ecs/components/Component.h"

namespace Game {
    namespace Unit {
        enum class Type {
            Red,
            Green,
            Blue,
        };
    }

    struct UnitColor: public Carrot::IdentifiableComponent<UnitColor> {
        Unit::Type color;

        explicit UnitColor(Unit::Type color): color(color) {}
    };
}
