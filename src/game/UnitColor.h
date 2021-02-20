//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include "ecs/components/Component.h"
#include "Unit.h"

namespace Game {
    struct UnitColor: public Carrot::IdentifiableComponent<Carrot::Transform> {
        Unit::Type color;

        explicit UnitColor(Unit::Type color): color(color) {}
    };
}
